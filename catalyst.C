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

#include  <time.h>
#include  <stdarg.h>
#include  <signal.h>


#include <sys/types.h>
#include <sys/stat.h>

#include <algorithm>
#include  <thread>
#include  <string>
#include  <vector>
#include  <deque>

using namespace std;


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
        return  counter++;
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
    int  time_limit;
    pid_t pid;


    // The wide search implementation
    int  task_index;   // 1,2,3,...
    int  next_wakeup;  // next_wakeup = current_wakeup + task_index
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
        if  ( duration()  > time_limit )
            return  true;
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

class SManager
{
private:
    string  prog, work_dir;
    bool   f_success_found;
    int  counter_tasks_created;
    bool  f_done;
    string  success_file;
    AbstractSequence  * seq_gen;

    /// Wide search implementation
    bool  f_wide_search;

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


    void  check_for_success();

    char  * get_success_file_name() {
        if  ( success_fn == NULL ) {
            string  s = work_dir + success_file;
            success_fn = strdup( s.c_str() );
        }
        return  success_fn;
    }

    SManager()
    {
        f_wide_search = false;
        success_fn = NULL;
        f_done = false;
        counter_tasks_created = 0;
        f_success_found = false;
        max_jobs_number = 1;
        f_scheduler_stop = false;
        seq_gen = NULL;
        g_timer = 0;
    }

    void wakeup_thread() {
    }

    void set_success( bool  f_val ) {
        f_success_found = f_val;
    }

    void set_program( const char  * _prog ) { prog = _prog; }
    void set_work_dir( const char  * _dir ) { work_dir = _dir; }
    void terminate_expired_tasks();

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
    //int   wait_on_queue();

    void  set_jobs_limit( int  jobs_limit )
    {
        max_jobs_number = jobs_limit;
    }


};

/*--- Start of code ---*/


SManagerPtr  create_scheduler( )
{
    static SManagerPtr  p_manager = NULL;

    if  ( p_manager != NULL )
        return  p_manager;

    p_manager = new SManager();
    assert( p_manager != NULL );

    return  p_manager;
}



void  SManager::check_for_success()
{
    //printf( "\n\n\n\n\nSUCC: %s\n", get_success_file_name() );
    if  ( is_file_exists( get_success_file_name() ) )
        f_success_found = true;
}



void  SManager::check_for_done_tasks()
{
    //printf( "void  SManager::check_for_done_tasks()\n" );
    check_for_success();
    for  ( int  ind  = active_tasks.size() - 1; ind >= 0; ind-- ) {
        Task  * p_task = active_tasks[ ind ];

        if  ( p_task->is_done() ) {
            if ( p_task->is_successful() )
                f_success_found = true;
            if  ( ! f_wide_search )
                delete   p_task;
            else {
                p_task->compute_next_wakeup();
                suspended_tasks.push_back( p_task );
                push_heap( suspended_tasks.begin(), suspended_tasks.end(),
                                TaskComparatorByWakeup() );

                for ( unsigned  k = 0; k < ( suspended_tasks.size() - 1 ); k++ ) {
                    if  ( suspended_tasks[ k ]->next_wakeup_time()
                          > suspended_tasks[ k + 1 ]->next_wakeup_time() ) {
                        printf( "\n\n\nSORTING ORDER WROGN\n" );
                        exit( -1 );
                    }
                }
            }

            active_tasks.erase( active_tasks.begin() + ind );
        }
    }
    //debug_myprintf( "Active tasks: %d\n", (int)active_tasks.size() );
}


void   kill_process( pid_t  cpid )
{
    char buff[1024] = "";

    //////////////////////////////////////////////////////////////////
    // Murder!!! For some strange reason to kill the process
    // and all tis children, we need to both use pkill and
    // kill. Don't ask me why..
    //
    // pkill is needed to kill all the child processes of the
    // process (what happens if ALG is a script.
    //////////////////////////////////////////////////////////////////
    //sprintf( buff, "pkill -P %d > /dev/null 2 >& 1", cpid );
    sprintf( buff, "pkill -P %d", cpid );
    system( buff );

    kill( cpid, SIGKILL );
}



void   SManager::terminate_expired_tasks()
{
    int  count = 0;

    check_for_success();
    //printf( "\n\n\n\nf_success_found: %d\n", (int)f_success_found );
    for  ( int  ind  = active_tasks.size() - 1; ind >= 0; ind-- ) {
        Task  * p_task = active_tasks[ ind ];

        if  ( ( ! f_success_found )  &&  ( ! p_task->is_expired() ) )
            continue;

        check_for_success();

        if  ( p_task->is_successful() )
            f_success_found = true;
        // XXX
        pid_t cpid = p_task->get_child_pid();
        if   ( f_success_found  ||  ( ! f_wide_search ) ) {
            count++;
            kill_process( cpid );

            check_for_success();

            p_task->set_done( true );
            continue;
        }

        // f_success_found == false  &&  f_wide_search = true
        assert( ! f_success_found );
        assert( p_task->is_expired() );
        assert( f_wide_search );

        ///suspend_process( cpid );

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

    tsk->set_time_limit( (int)seq_gen->next() );
    tsk->set_manager( this );

    counter_tasks_created++;
    char buf[1024];
    sprintf( buf, "/%06d", counter_tasks_created );

    string new_dir =  work_dir + buf;
    mkdir( new_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH );

    string  out_cmd = prog + " " + new_dir;
    tsk->set_command( prog );
    tsk->set_dir( new_dir );

    active_tasks.push_back( tsk );

    tsk->launch();
}


void  SManager::spawn_tasks()
{
    check_for_success();

    if  ( f_success_found )
        return;

    while  ( (int)( active_tasks.size() ) < max_jobs_number ) {
        //printf( "ST Spawnning...\n" );
        spawn_single_task();
    }
}


void   SManager::main_loop()
{
    //printf( "main loop entered...\n" );
    do {
        //printf( "ML Looping...\n" );
        check_for_done_tasks();
        //printf( "A Looping...\n" );
        if  ( is_found_success() )
            break;
        //printf( "ML B Looping...\n" );
        terminate_expired_tasks();
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
    terminate_expired_tasks();
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

#define  STATUS_RUNNING 73
#define  STATUS_DONE    74
#define  STATUS_PAUSED  75


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
        printf( "BINGO!\n" );
        f_done = true;
        return;
    }

    //printf( "TSK: [%s]  %d\n", command.c_str(), (int)time_limit );
    //fflush( stdout );
    //int  val = system( command.c_str() );


    char * prog_name = strdup(  command.c_str() );
    char * prog_dir = strdup( dir.c_str() );
    char * success_fn = p_manager->get_success_file_name();
    char *argv[] = { prog_name, prog_dir, success_fn, NULL};
    posix_spawnattr_t attr;

    // Initialize spawn attributes
    posix_spawnattr_init(&attr);

    // Set flags if needed, for example, to specify the scheduling policy
    // posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSCHEDULER);

    // Spawn a new process
    //printf( "New process started!\n" );
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


int  main(int   argc, char*   argv[])
{
    printf( "time: %ld\n", time( NULL ) );
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
    SManagerPtr p_manager = create_scheduler();

    if  ( argc == 4 ) {
        if  ( strcasecmp( argv[ 1 ], "-a" ) != 0 )
            usage();
        p_manager->set_program( argv[ 2 ] );
        p_manager->set_work_dir( argv[ 3 ] );
        p_manager->set_wide_search( true );
        p_manager->set_seq_generator( new  SequencePlusOne() );
    } else {
        if  ( argc != 3 )
            usage();
        p_manager->set_program( argv[ 1 ] );
        p_manager->set_work_dir( argv[ 2 ] );
        p_manager->set_seq_generator( new  SequenceCounter() );
    }

    unsigned int numThreads = std::thread::hardware_concurrency();
    p_manager->set_threads_num(  numThreads );

    p_manager->set_success_file( "success.txt" );

    //    printf( "bogi B\n" );

    p_manager->main_loop();

    printf( "Everything is done???\n" );

    return  0;
}

/* catalyst.C - End of File ------------------------------------------*/
