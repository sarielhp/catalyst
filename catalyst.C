/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
 * catalyst.C -
 *     A catastrophe avoiding scheduler.
\*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/

#include  <sys/stat.h>
#include  <stdlib.h>
#include  <stdio.h>
#include  <assert.h>
#include  <pthread.h>
#include  <string.h>
#include  <unistd.h>
#include  <spawn.h>
#include  <sys/wait.h>

#include  <time.h>
#include  <stdarg.h>
#include  <signal.h>


#include <sys/types.h>
#include <sys/stat.h>

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


class   CounterSequence : public AbstractSequence
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
    CounterSequence()
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


/////////////////////////////////////////////////////////////////////////////



inline bool is_file_exists(const std::string& name) {
  struct stat buffer;
  return (stat (name.c_str(), &buffer) == 0);
}


/*--- Constants ---*/

void  * task_do( void  * this_ptr );
void  * main_scheduler_loop( void  * );

class SManager;
typedef class SManager   * SManagerPtr;

/*--- Static types ---*/
class  Task
{
private:
    SManagerPtr   p_manager;
    string  command, dir;
    int   thread_id;
    bool  f_done, f_success;
    time_t  start_time;
    int  time_limit;
    pid_t pid;

public:
    pthread_t  thread;
public:
    Task() {
        thread_id = 0;
        f_done = f_success = false;
        p_manager = NULL;
        f_done = f_success = false;
    }

    pid_t  get_child_pid() { return  pid; }

    int  get_time_limit() { return time_limit; }
    void  set_command( string  s ) { command = s; }
    void  set_dir( string  s ) { dir = s; }

    void  set_time_limit( int  _limit ) { time_limit = _limit; }

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
    void  execute();

    const  char   * get_cmd() { return  command.c_str(); }
    void  run() {
        int  ret;

        f_done = false;
        ret = pthread_create( &thread, NULL, task_do, (void *)this );
        if  ( ret != 0 ) {
            fprintf( stderr, "Unable to create thread for [%s]\n",
                     command.c_str() );
            fflush( stderr );
        }
    }

    Task( const char  * _cmd ) {
        command = string( _cmd );
        thread_id = 0;
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


typedef std::deque<Task>  deque_tasks;

class SManager
{
private:
    string  prog, work_dir;
    bool   f_success_found;
    int  counter_tasks_created;
    bool  f_done;
    string  success_file;

public:
    void  set_success_file( string s ) { success_file = s; }

    int   max_jobs_number;
    bool  f_scheduler_created;
    bool  f_scheduler_stop;
    //deque_tasks  tasks;
    std::vector<Task *>   active_tasks;
    pthread_mutex_t   mutex_tasks;
    pthread_cond_t    scheduler_awake_cond;
    pthread_mutex_t   scheduler_awake_mutex;
    pthread_t  scheduler_thread;
    char * success_fn;

    AbstractSequence  * seq_gen;

    bool  is_found_success() {
        return  f_success_found;
    }

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
        success_fn = NULL;
        f_done = false;
        counter_tasks_created = 0;
        f_success_found = false;
        max_jobs_number = 1;
        f_scheduler_created = false;
        f_scheduler_stop = false;
        mutex_tasks  = PTHREAD_MUTEX_INITIALIZER;
        scheduler_awake_cond  = PTHREAD_COND_INITIALIZER;
        scheduler_awake_mutex  = PTHREAD_MUTEX_INITIALIZER;
        seq_gen = NULL;
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
    int   wait_on_queue();
    //    int   execute_command( const char  * cmd  );

    void  set_jobs_limit( int  jobs_limit )
    {
        max_jobs_number = jobs_limit;
    }

    void  start_main_thread() {
        pthread_create( &(scheduler_thread), NULL,
                        &main_scheduler_loop, (void *)this );
    }

};

/*--- Start of code ---*/
void  * task_do( void  * this_ptr )
{
    Task  * p_task;

    p_task = (Task *)this_ptr;

    // Make sure the thread is cancel friendly
    int prevType;
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &prevType);

    p_task->record_start_time();
    printf( "TIME limit: %d\n", p_task->get_time_limit() );

    p_task->execute();

    //debug_myprintf( "TASK done!\n" );

    p_task->set_done( true );

    SManagerPtr  p_sched = p_task->manager();//p_scheduler;

    // Wake up the scheduler!!!
    pthread_cond_broadcast( & p_sched->scheduler_awake_cond );
    //pthread_cond_broadcast( & p_sched->scheduler_awake_cond );

    return  NULL;
}


SManagerPtr  create_scheduler( AbstractSequence  & seq_gen )
{
    static SManagerPtr  p_manager = NULL;

    if  ( p_manager != NULL )
        return  p_manager;

    p_manager = new SManager();
    assert( p_manager != NULL );
    p_manager->seq_gen = &seq_gen;

    p_manager->f_scheduler_created = true;

    return  p_manager;
}

void  SManager::check_for_success()
{
    if  ( is_file_exists( get_success_file_name() ) )
        f_success_found = true;
}



void  SManager::check_for_done_tasks()
{
    //printf( "void  SManager::check_for_done_tasks()\n" );
    check_for_success();
    for  ( int  ind  = active_tasks.size() - 1; ind >= 0; ind-- )
        if  ( active_tasks[ ind ]->is_done() ) {
            if (  active_tasks[ ind ]->is_successful() )
                f_success_found = true;
            pthread_mutex_lock( &mutex_tasks );
            delete   active_tasks[ ind ];
            active_tasks.erase( active_tasks.begin() + ind );
            pthread_mutex_unlock( &mutex_tasks );
        }
    debug_myprintf( "Active tasks: %d\n", (int)active_tasks.size() );
}


void   SManager::terminate_expired_tasks()
{
    check_for_success();
    //printf( "\n\n\n\nf_success_found: %d\n", (int)f_success_found );
    for  ( int  ind  = active_tasks.size() - 1; ind >= 0; ind-- ) {
        if  ( f_success_found  ||  active_tasks[ ind ]->is_expired() ) {
            if  ( active_tasks[ ind ]->is_successful() )
                f_success_found = true;

            //printf( "Canceling?\n" ); fflush( stdout );

            //printf( "KILL Process ID: %d\n",
            //          (int)active_tasks[ ind ]->get_child_pid() );
            kill( active_tasks[ ind ]->get_child_pid(), SIGTERM ); //SIGKILL );
            kill( active_tasks[ ind ]->get_child_pid(), SIGKILL ); //SIGKILL );

            //pthread_cancel( active_tasks[ ind ]->thread );
            pthread_join( active_tasks[ ind ]->thread, NULL );

            pthread_mutex_lock( &mutex_tasks );
            delete   active_tasks[ ind ];
            active_tasks.erase( active_tasks.begin() + ind );
            pthread_mutex_unlock( &mutex_tasks );
        }
    }
}


void  SManager::spawn_single_task()
{
    if  ( f_success_found )
        return;

    if  ( (int)( active_tasks.size() ) >= max_jobs_number )
        return;

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

    pthread_mutex_lock( &mutex_tasks );
    //tasks.pop_front();
    active_tasks.push_back( tsk );
    pthread_mutex_unlock( &mutex_tasks );

    tsk->run();
}


void  SManager::spawn_tasks()
{
    check_for_success();

    if  ( f_success_found )
        return;

    while  ( (int)( active_tasks.size() ) < max_jobs_number ) {
        spawn_single_task();
    }
}


void   SManager::main_loop()
{
    //printf( "main loop entered...\n" );
    do {
        //printf( "Looping...\n" );
        check_for_done_tasks();
        if  ( is_found_success() )
            break;
        terminate_expired_tasks();
        if  ( is_found_success() )
            break;
        spawn_tasks();
        if  ( is_found_success() )
            break;

        sleep( 1 );

    }  while ( ! is_found_success() );
    terminate_expired_tasks();
    f_done = true;
}


void  * main_scheduler_loop( void  * _p_manager)
{
    SManager   * p_manager = (SManager *)_p_manager;

    p_manager->main_loop();

    return  NULL;
}


int  SManager::wait_on_queue()
{
    pthread_cond_broadcast( &scheduler_awake_cond );
    do {
        //printf( "************** WAITING!!!\n" ); fflush( stdout );
        pthread_cond_broadcast( &scheduler_awake_cond );
        pthread_mutex_lock( &scheduler_awake_mutex );
        if   ( active_tasks.size() > 0 )  {
            pthread_cond_wait( &scheduler_awake_cond,
                               &scheduler_awake_mutex );
        }
        pthread_mutex_unlock( &scheduler_awake_mutex );
    }  while  ( active_tasks.size() > 0 );

    return  0;
}


void  Task::execute()
{
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
    int status;
    posix_spawnattr_t attr;

    // Initialize spawn attributes
    posix_spawnattr_init(&attr);

    // Set flags if needed, for example, to specify the scheduling policy
    // posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSCHEDULER);

    // Spawn a new process
    if (posix_spawn(&pid, command.c_str(), NULL, &attr, argv, NULL) != 0) {
        perror("spawn failed");
        exit(EXIT_FAILURE);
    }
    //printf( "PROCESS: %d\n", pid );

    //int status;
    //    wpid = waitpid( pid, &status, WUNTRACED | WCONTINUED);

    do {
        //printf( "Wait...\n" );
        pid_t  wpid = waitpid( pid, &status, WUNTRACED | WCONTINUED );
        //printf( "\n\n\nXXXWait done...\n" );
        if (wpid == -1) {
            // Child process is dead. Bye bye...
            //perror("waitpid");
            //exit(EXIT_FAILURE);
            break;
        }

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
        printf( "WHILE...\n" );
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));

    /*sleep( 1 );

    if  ( is_file_exists( dir + "success.txt" ) ) {
        printf( "DONE DONE DONE DONE!\n" );
        p_manager->set_success( true  );
        f_success = true;
        }*/

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

    CounterSequence  cnt;
    /*
    for  ( int  i = 0; i <= 10; i++ ) {
        printf( "%d\n", cnt.next() );
        }*/


    if ( argc != 3 )
        usage();

    // Just terminate child processes when they are done, don't let
    // them become zombies...
    struct sigaction arg;
    arg.sa_handler=SIG_IGN;
    arg.sa_flags=SA_NOCLDWAIT; // Never wait for termination of a child process.

    sigaction(SIGCHLD, &arg, NULL);



    CounterSequence   seq_gen;
    SManagerPtr p_manager = create_scheduler( seq_gen );
    p_manager->set_program( argv[ 1 ] );
    p_manager->set_work_dir( argv[ 2 ] );

    printf( "bogi\n" );

    unsigned int numThreads = std::thread::hardware_concurrency();
    p_manager->set_threads_num(  numThreads );

    p_manager->set_success_file( "success.txt" );

    printf( "bogi B\n" );
    p_manager->start_main_thread();

    while  ( ! p_manager->is_done() ) {
        printf( "Sleeping...\n" );
        sleep( 1 );
    }

    printf( "Everything is done???\n" );

    return  0;
}

/* catalyst.C - End of File ------------------------------------------*/
