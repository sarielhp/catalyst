/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
 * catalyst.C -
 *     A catastrophe avoiding scheduler.
\*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/

#include  <sys/stat.h>
#include  <stdlib.h>
#include  <stdio.h>
#include  <assert.h>
//#include  <pthread.h>
#include  <string.h>
#include  <unistd.h>
#include  <spawn.h>
#include  <sys/wait.h>
#include  <limits.h>

#include  <time.h>
#include  <stdarg.h>
#include  <signal.h>


#include <sys/types.h>
#include <sys/stat.h>

#include  <algorithm>
#include  <thread>
#include  <string>
#include  <vector>
#include  <deque>
#include  <filesystem>

using namespace std;

#define  STATUS_RUNNING 73
#define  STATUS_DONE    74
#define  STATUS_PAUSED  75


static bool  global_f_verbose = true;

void  myprintf_inner( const char  * out )
{
    fputs( out, stdout );
}


void    debug_myprintf( const char  * format, ... )
{
    //char  out[ 10*LINE_SIZE ];

    if  ( ! global_f_verbose )
        return;

    string  out;
    va_list ap;
    va_list apStrLen;
    int  length;

    va_start(ap, format);

    va_copy( apStrLen, ap);
    length = vsnprintf(NULL, 0, format, apStrLen) + 100;
    va_end(apStrLen);

    char buf[ length +2 ];
    vsnprintf( buf, length + 1, format, ap);

    out = string( buf );

    //string_printf_inner( out, format, ap );
    va_end(ap);

    //printf( "out: %s\n", out.c_str() );
    fputs( out.c_str(), stdout );
}


/////////////////////////////////////////////////////////////////////////////
/// Sequence abstract - generates sequences of times we should run each
/// algorithm on.

class  AbstractSequence
{
public:
    virtual  int  next() {return 0;}
};


class   SequenceCounter : public AbstractSequence
{
private:
    int  counter;
    vector<int> stack;

    int  stack_pop() {
        auto last = stack.back();
        stack.pop_back();
        return  last;
    }
public:
    SequenceCounter()
    {
        counter = 0;
    }

    virtual  int  next()
    {
        if  ( ! stack.empty() )
            return  stack_pop();

        counter++;
        int pw = ( 1 + ((counter - 1) ^ counter) ) >> 1;
        while  ( pw > 0 ) {
            stack.push_back( pw );
            pw = pw >> 1;
        }

        return  stack_pop();
    }
};


class   SequencePlusOne : public AbstractSequence
{
private:
    int  counter;

public:
    SequencePlusOne()
    {
        counter = 0;
    }

    virtual  int  next()
    {
        return  ++counter;
    }
};


class   SequenceMaxInt : public AbstractSequence
{
private:

public:
    SequenceMaxInt()
    {
    }

    virtual  int  next()
    {
        return  INT_MAX;
    }
};


/////////////////////////////////////////////////////////////////////////////



inline bool   is_file_exists(const std::string& name) {
  struct stat   buffer;
  return (stat( name.c_str(), &buffer ) == 0);
}


/*--- Constants ---*/

class SManager;
typedef class SManager   * SManagerPtr;

/*--- Static types ---*/
class  Task
{
private:
    SManagerPtr   p_manager;
    string  command, dir;
    //int   thread_id;
    bool  f_done, f_success;
    time_t  start_time;
    int  time_limit, id;
    pid_t pid;


    // The wide search implementation
    int  next_wakeup;  // next_wakeup = current_wakeup + time_delta
    int  time_delta;   // Time to add for next wakeup

public:
    pthread_t  thread;
public:
    Task() {
        //thread_id = 0;
        f_done = f_success = false;
        p_manager = NULL;
        f_done = f_success = false;
        next_wakeup = 0;
        time_delta = 0;
    }

    void   set_time_delta( int  _dlt ) { time_delta = _dlt; }
    void   set_id( int  _id ) { id = _id; }
    int    process_status();
    void   set_done() { f_done = true; }

    pid_t  get_child_pid() { return  pid; }

    int  next_wakeup_time() const { return   next_wakeup; }

    int  get_time_limit() { return time_limit; }
    void  set_command( string  s ) { command = s; }
    void  set_dir( string  s ) { dir = s; }

    void  set_time_limit( int  _limit ) { time_limit = _limit; }

    void  compute_next_wakeup() {
        assert( time_delta > 0 );
        next_wakeup += time_delta;
    }

    bool  is_successful() { return  f_success; }
    bool  is_expired()
    {
        if  ( f_done  ||  f_success )
            return  true;
        if  ( duration()  >= time_limit ) {
            //printf( "EXPIRED!\n" );
            return  true;
        }

        if  ( process_status() ==  STATUS_DONE ) {
            f_done = true;
            return  true;
        }

        //printf( "FALSE\n" );
        return  false;
    }


    void  set_manager( SManagerPtr   _p_manager )
    {
        p_manager = _p_manager;
    }

    void  record_start_time()
    {
        start_time = time( NULL );
    }

    int  duration()
    {
        return  (int)( time( NULL ) - start_time );
    }

    SManagerPtr   manager()
    {
        return  p_manager;
    }
    void  launch();

    const  char   * get_cmd() { return  command.c_str(); }

    Task( const char  * _cmd ) {
        id = 0;
        command = string( _cmd );
        f_done = false;
    }

    ~Task() {
        //if   ( cmd != NULL )
        //   free( cmd );
        command = "";
    }

    void  set_done( bool  val ) { f_done = val; }
    bool  is_done()  { return  f_done; }
};


class TaskComparatorByWakeup {
public:
    bool operator()( const Task  * a, const Task  * b) const {
        assert( ( a != NULL )  &&  ( b != NULL ) );

        return a->next_wakeup_time() > b->next_wakeup_time(); // For a max heap
    }
};


typedef std::deque<Task>  deque_tasks;

// XManager
class SManager
{
private:
    string  prog, work_dir;
    bool   f_success_found;
    int  counter_tasks_created;
    bool  f_done;
    string  success_file;
    AbstractSequence  * seq_gen;

    int  time_counter;

    /// Wide search implementation
    bool  f_wide_search;
    bool  f_parallel_search;

    int   max_jobs_number;
    bool  f_scheduler_stop;
        //deque_tasks  tasks;
    std::vector<Task *>   active_tasks;
    std::vector<Task *>   suspended_tasks;
    pthread_mutex_t   mutex_tasks;
    pthread_cond_t    scheduler_awake_cond;
    pthread_mutex_t   scheduler_awake_mutex;
    pthread_t  scheduler_thread;
    char * success_fn;
    int  g_timer;

public:
    void  set_success_file( string s ) { success_file = s; }
    bool  is_found_success() {
        return  f_success_found;
    }
    void  set_seq_generator( AbstractSequence  * _seq_gen ) {
        seq_gen = _seq_gen;
    }

    void  set_wide_search( bool  flag ) { f_wide_search = flag; }
    void  set_parallel_search( bool  flag ) { f_parallel_search = flag; }


    void  check_for_success();

    char  * get_success_file_name() {
        if  ( success_fn == NULL ) {
            string  s = success_file;
            success_fn = strdup( s.c_str() );
        }
        return  success_fn;
    }

    SManager()
    {
        time_counter = 0;
        f_parallel_search = f_wide_search = false;
        success_fn = NULL;
        f_done = false;
        counter_tasks_created = 0;
        f_success_found = false;
        max_jobs_number = 1;
        f_scheduler_stop = false;
        seq_gen = NULL;
        g_timer = 0;
    }

    const char   * get_mode_str();
    bool  slots_available();
    void wakeup_thread() {
    }

    void set_success( bool  f_val ) {
        f_success_found = f_val;
    }

    void   set_program( const char  * _prog ) { prog = _prog; }
    void   set_work_dir( const char  * _dir ) { work_dir = strdup( _dir ); }
    string   get_work_dir() { return  work_dir; }
    void   handle_expired_tasks();
    void   kill_all_tasks();

    bool  is_done()
    {
        return  f_done;
    }
    bool  is_parallel() {
        return  max_jobs_number > 1;
    }
    void  set_threads_num( int  n ) { max_jobs_number = n; }
    void  spawn_single_task();
    void  spawn_tasks();
    void  check_for_done_tasks();
    void  main_loop();

    void  suspend_process( int  ind );
    void  resume_first_process();

    void  verify_suspended_tasks_heap();
};

/*--- Start of code ---*/


void  SManager::check_for_success()
{
    //printf( "\n\n\n\n\nSUCC: %s\n", get_success_file_name() );
    if  ( is_file_exists( get_success_file_name() ) )
        f_success_found = true;
}



void  SManager::check_for_done_tasks()
{
    check_for_success();
    for  ( int  ind  = active_tasks.size() - 1; ind >= 0; ind-- ) {
        Task  * p_task = active_tasks[ ind ];

        if  ( p_task->is_done() ) {
            if ( p_task->is_successful() )
                f_success_found = true;
            delete   p_task;
            active_tasks.erase( active_tasks.begin() + ind );
        }
    }
    //printf( "\n\n\nActive tasks: %d\n", (int)active_tasks.size() );
    //debug_myprintf( "Active tasks: %d\n", (int)active_tasks.size() );
}


//////////////////////////////////////////////////////////////////
// Murder!!! For some strange reason to kill the process
// and all its children, we need to both use pkill and
// kill. Don't ask me why..
//
// pkill is needed to kill all the child processes of the
// process (what happens if ALG is a script.
//////////////////////////////////////////////////////////////////

void  send_signal( pid_t  cpid, int sig, const char  * sig_str )
{
    char  buff[ 1024 ];


    sprintf( buff, "pkill -%s -P %d", sig_str, cpid );
    system( buff );

    kill( cpid, sig );
}


void   kill_process( pid_t  cpid )
{
    send_signal( cpid, SIGKILL, "SIGKILL" );
}


void    SManager::verify_suspended_tasks_heap()
{
    for ( unsigned  k = 0; k < ( suspended_tasks.size() - 1 ); k++ ) {
        unsigned u = 2*k+1;
        if  ( u >= suspended_tasks.size() )
            continue;
        if  ( suspended_tasks[ k ]->next_wakeup_time()
              > suspended_tasks[ u ]->next_wakeup_time() ) {
            printf( "\n\n\nSORTING ORDER WROGN\n" );
            exit( -1 );
        }
        u = 2*k+2;
        if  ( u >= suspended_tasks.size() )
            continue;
        if  ( suspended_tasks[ k ]->next_wakeup_time()
              > suspended_tasks[ u ]->next_wakeup_time() ) {
            printf( "\n\n\nSORTING ORDER WROGN\n" );
            exit( -1 );
        }
    }
}


void   SManager::suspend_process( int  ind )
{
    Task  * p_task = active_tasks[ ind ];

    //----------------------------------------------------
    // Suspending the process: The low level stuff
    send_signal( p_task->get_child_pid(), SIGTSTP, "SIGTSTP" );

    // Moving the task to suspended array...
    // Suspending a process is a bit tricky, since we have to keep
    // them sorted by wakeup time.
    active_tasks.erase( active_tasks.begin() + ind );

    p_task->compute_next_wakeup();
    suspended_tasks.push_back( p_task );
    push_heap( suspended_tasks.begin(), suspended_tasks.end(),
               TaskComparatorByWakeup() );

    // Verify suspended tasks are sorted correct in the *heap*
    verify_suspended_tasks_heap();
}


void   SManager::resume_first_process()
{
    assert( f_wide_search );
    assert( suspended_tasks.size() > 0 );

    Task * p_task = suspended_tasks.front() ;

    // Update the start time of the process
    p_task->record_start_time();

    //----------------------------------------------------
    // Suspending the process: Low level stuff
    send_signal( p_task->get_child_pid(), SIGCONT, "SIGCONT" );

    /// Moving it from suspend to resumed tasks...
    active_tasks.push_back( p_task );

    pop_heap( suspended_tasks.begin(), suspended_tasks.end(),
              TaskComparatorByWakeup() );
    suspended_tasks.pop_back();
    verify_suspended_tasks_heap();
}




void   SManager::kill_all_tasks()
{
    int  count = 0;
    for  ( int  ind  = active_tasks.size() - 1; ind >= 0; ind-- ) {
        //printf( "IND: %d\n", ind );  fflush( stdout );
        Task  * p_task = active_tasks[ ind ];
        kill_process( p_task->get_child_pid() );
        active_tasks.erase( active_tasks.begin() + ind );
        delete p_task;
        count++;
    }
    for  ( int  jnd  = suspended_tasks.size() - 1; jnd >= 0; jnd-- ) {
        //printf( "JNDx: %d\n", jnd ); fflush( stdout );

        Task  * p_task = suspended_tasks[ jnd ];
        pid_t   cpid = p_task->get_child_pid();

        /// First resume it...
        /// ... then kill it.
        send_signal( cpid, SIGCONT, "SIGCONT" );
        send_signal( cpid, SIGKILL, "SIGKILL" );

        //kill_process( p_task->get_child_pid() );
        suspended_tasks.erase( suspended_tasks.begin() + jnd );

        delete p_task;
        count++;
    }
    if  ( count > 0 )
        printf( "Killed %d processes\n", count );
}


void   SManager::handle_expired_tasks()
{
    int  count = 0;

    check_for_success();
    if   ( f_success_found )
        return;

    //printf( "handle_expired_tasks\n" );
    //printf( "\n\n\n\nf_success_found: %d\n", (int)f_success_found );
    for  ( int  ind  = active_tasks.size() - 1; ind >= 0; ind-- ) {
        Task  * p_task = active_tasks[ ind ];

        if   ( f_success_found )
            return;
        if   ( ! p_task->is_expired() )
            continue;
        ///printf( "Expired task????\n" );

        check_for_success();

        if  ( p_task->is_successful() )
            f_success_found = true;

        pid_t cpid = p_task->get_child_pid();
        if   ( f_success_found  ||  ( ! f_wide_search ) ) {
            count++;
            kill_process( cpid );

            check_for_success();

            p_task->set_done( true );
            continue;
        }
        if   ( f_parallel_search )
            continue;

        // f_success_found == false  &&  f_wide_search = true
        assert( ! f_success_found );
        assert( p_task->is_expired() );
        assert( f_wide_search );
        ///printf( "Suspending process!\n" );
        suspend_process( ind );
    }
    if  ( count > 0 )
        printf( "Killed %d processes\n", count );
}


void  SManager::spawn_single_task()
{
    if  ( f_success_found )
        return;

    if  ( (int)( active_tasks.size() ) >= max_jobs_number )
        return;

    //printf( "New task created!\n" );
    Task  * tsk = new  Task();

    counter_tasks_created++;

    // We run a process for one second if we are in the wide search mode...
    if  ( f_wide_search ) {
        tsk->set_id( counter_tasks_created );
        tsk->set_time_limit( 1 );
        tsk->set_time_delta( (int)seq_gen->next() );
        //printf( "XXXX\n" ); fflush( stdout );
        tsk->compute_next_wakeup();
        //printf( "ZZZZ\n" ); fflush( stdout );
    } else
        tsk->set_time_limit( (int)seq_gen->next() );
    tsk->set_manager( this );

    char buf[1024];
    sprintf( buf, "%06d", counter_tasks_created );

    string new_dir;
    if  ( work_dir.back() == '/' )
        new_dir =  work_dir + buf;
    else
        new_dir =  work_dir + "/" + buf;

    mkdir( new_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH );

    string  out_cmd = prog + " " + new_dir;
    tsk->set_command( prog );
    tsk->set_dir( new_dir );

    active_tasks.push_back( tsk );

    tsk->launch();
}


bool  SManager::slots_available()
{
    return   ( (int)( active_tasks.size() ) < max_jobs_number );
}


void  SManager::spawn_tasks()
{
    check_for_success();

    if  ( f_success_found )
        return;
    if  ( ! f_wide_search ) {
        while  ( slots_available() ) {
            spawn_single_task();
        }
        return;
    }

    assert( f_wide_search ); // Just for clarity

    // If no slot is available, then we have to wait for some active
    // tasks to be suspended...
    if  ( ! slots_available() )
        return;
    while  ( slots_available() ) {
        if  ( suspended_tasks.size() == 0 ) {
            time_counter++;   // TIME forward by one unit
            spawn_single_task();
            continue;
        }

        assert( suspended_tasks.size() > 0 );

        int  future = time_counter + 1;
        if  ( suspended_tasks[ 0 ]->next_wakeup_time() < future ) {
            resume_first_process();
            continue;
        }

        // Everything is in the future, we might as well move into it...
        time_counter++;   // TIME forward by one unit
        spawn_single_task();
    }
}


const char   * SManager::get_mode_str()
{
    if  ( f_wide_search )
        return "Wide search";
    if  ( f_parallel_search )
        return "Parallel search";

    return   "Parallel Counter";
}


void   SManager::main_loop()
{
    printf( "MODE               : %s\n", get_mode_str() );
    printf( "# of parallel jobs : %d\n", max_jobs_number );

    do {
        printf( "Active: %4d   suspended: %4d   SIM TIME: %6d\n",
                (int)active_tasks.size(),
                (int)suspended_tasks.size(),
                time_counter );

        //printf( "ML Looping...\n" );
        check_for_done_tasks();
        //printf( "A Looping...\n" );
        if  ( is_found_success() )
            break;
        //printf( "ML B Looping...\n" );
        handle_expired_tasks();
        if  ( is_found_success() )
            break;

        //printf( "ML C Looping...\n" );
        spawn_tasks();
        //printf( "D Looping...\n" );
        if  ( is_found_success() )
            break;

        //printf( "E Looping...\n" );
        sleep( 1 );

    }  while ( ! is_found_success() );
    kill_all_tasks();
    f_done = true;
}


void  waitpid_error( int  status )
{
    if (WIFEXITED(status)) {
        printf("child exited, status=%d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("child killed (signal %d)\n", WTERMSIG(status));
    } else if (WIFSTOPPED(status)) {
        printf("child stopped (signal %d)\n", WSTOPSIG(status));
    } else if (WIFCONTINUED(status)) {
        printf("child continued\n");
    } else {    /* Non-standard case -- may never happen */
        printf("Unexpected status (0x%x)\n", status);
    }
}



int    Task::process_status()
{
    int status, endID;

    endID = waitpid( pid, &status, WNOHANG|WUNTRACED);
    if (endID == -1) {            /* error calling waitpid       */
        perror("waitpid error");
        exit(EXIT_FAILURE);
    }
    if (endID == 0) /* child still running         */
        return  STATUS_RUNNING;

    if (endID == pid ) {  /* child ended                 */
        if (WIFEXITED(status)) {
            f_done = true;
            return  STATUS_DONE;
        }
        if (WIFSIGNALED(status)) {
            printf("Child ended because of an uncaught signal.n");
            assert( false );
            return  -1;
        }
        if (WIFSTOPPED(status))
            return  STATUS_PAUSED;
    }

    return -1;
}


void  Task::launch()
{
    record_start_time();

    if  ( p_manager->is_found_success() ) {
        f_done = true;
        return;
    }

    char * prog_name = strdup(  command.c_str() );
    char * prog_dir = strdup( dir.c_str() );
    char * success_fn = p_manager->get_success_file_name();
    printf( "SUCC: [%s]\n", success_fn );

    char *argv[] = { prog_name, prog_dir, success_fn, NULL};
    posix_spawnattr_t attr;

    // Initialize spawn attributes
    posix_spawnattr_init(&attr);

    // Set flags if needed, for example, to specify the scheduling policy
    // posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSCHEDULER);

    // Spawn a new process
    //printf( "New process started!\n" );
    printf( "CMD: %s\n", command.c_str() );
    if (posix_spawn(&pid, command.c_str(), NULL, &attr, argv, NULL) != 0) {
        perror("spawn failed");
        exit(EXIT_FAILURE);
    }
}


void  usage()
{
    printf( "Catalyst\n\t"
            "Job scheduler for multi threaded system to avoid "
            "catastrophic running times for Las Vegas Algorithms.\n" );
    exit( -1 );
}

void  append_slash( char  * buf )
{
    int  len = strlen( buf );
    if  ( ( len == 0 )  ||  ( buf[ len - 1 ] != '/' ) )
        strcat( buf, "/" );
}


int  main(int   argc, char*   argv[])
{
    //printf( "time: %ld\n", time( NULL ) );
    if  ( argc == 0 )
        usage();

    SequenceCounter  cnt;
    /*
      for  ( int  i = 0; i <= 10; i++ ) {
      printf( "%d\n", cnt.next() );
      }
    */

    // Just terminate child processes when they are done, don't let
    // them become zombies...
    struct sigaction arg;
    arg.sa_handler=SIG_IGN;
    arg.sa_flags=SA_NOCLDWAIT; // Never wait for termination of a child process.

    sigaction(SIGCHLD, &arg, NULL);

    //SequenceCounter   seq_gen;
    SManagerPtr p_manager = new SManager();
    assert( p_manager != NULL );

    if  ( argc == 4 ) {
        bool  f_parallel = ( strcasecmp( argv[ 1 ], "-p" ) == 0 );
        bool  f_wide = ( strcasecmp( argv[ 1 ], "-a" ) == 0 );
        if  ( (!f_parallel)  &&  ( ! f_wide ) )
            usage();


        p_manager->set_program( argv[ 2 ] );
        p_manager->set_work_dir( argv[ 3 ] );
        //printf( "WORK DIR: %s\n", argv[ 3 ] );
        if  ( f_wide ) {
            p_manager->set_wide_search( true );
            p_manager->set_seq_generator( new  SequencePlusOne() );
        }
        if  ( f_parallel ) {
            p_manager->set_parallel_search( true );
            p_manager->set_seq_generator( new  SequenceMaxInt() );
        }
    } else {
        if  ( argc != 3 )
            usage();
        p_manager->set_program( argv[ 1 ] );
        p_manager->set_work_dir( argv[ 2 ] );
        printf( "WORK DIR: %s\n", argv[ 2 ] );
        p_manager->set_seq_generator( new  SequenceCounter() );
    }

    unsigned int numThreads = std::thread::hardware_concurrency();
    p_manager->set_threads_num(  numThreads );
    //p_manager->set_threads_num(  1 );

    char  buf[ 1024 ];
    char  out_success_fn[ 1024 ];

    strcpy( buf, p_manager->get_work_dir().c_str() );

    //printf( "BUFY: %s\n", buf );


    std::filesystem::remove_all( buf );

    mkdir( buf, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH );

    ///printf( "BUFX: %s\n", buf );

    char  work_dir[ 1024 ];

    char  * rpath = realpath( buf, out_success_fn );
    assert( rpath != NULL );
    append_slash( out_success_fn );
    strcpy( work_dir, out_success_fn );

    p_manager->set_work_dir( work_dir );

    strcat( out_success_fn, "success.txt" );


    printf( "Work directory : %s\n", work_dir );
    printf( "Success file   : %s\n", out_success_fn );

    p_manager->set_success_file( out_success_fn );

    //    printf( "bogi B\n" );

    p_manager->main_loop();

    printf( "Everything is done???\n" );

    return  0;
}


/* catalyst.C - End of File ------------------------------------------*/
