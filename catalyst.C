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
#include  <argp.h>

#include  <time.h>
#include  <stdarg.h>
#include  <signal.h>


#include <libproc2/misc.h>
#include  <sys/types.h>
#include  <sys/stat.h>

#include  <algorithm>
#include  <thread>
#include  <string>
#include  <vector>
#include  <deque>
#include  <filesystem>
#include  <random>
#include  <chrono>

#include <libproc2/pids.h>

/////////////////////////////////////////////////////////////
using namespace std;

#define  STATUS_RUNNING 73
#define  STATUS_DONE    74
#define  STATUS_PAUSED  75



///////////////////////////////////////////////////////////////

void  append_slash( char  * buf )
{
    int  len = strlen( buf );
    if  ( ( len == 0 )  ||  ( buf[ len - 1 ] != '/' ) )
        strcat( buf, "/" );
}



////////////////////////////////////////////////////////////////

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


class   SequenceRepeatVal : public AbstractSequence
{
private:
    int val;
    
public:
    SequenceRepeatVal( int  _val )
    {
        val = _val;
    }

    virtual  int  next()
    {
        return  val; //INT_MAX;
    }
};



class   SequenceRandom : public AbstractSequence
{
private:
    std::random_device rd; // Hardware-based random number generator
    std::uniform_int_distribution<int> distribution;
    std::mt19937 generator;

    int   coin_flip() {
        if  ( distribution(generator) >= 51 )
            return  1;
        else
            return  0;
    }

public:
    SequenceRandom()
    {
        generator = std::mt19937(rd()); // Mersenne Twister engine seeded with rd()
        distribution  =  std::uniform_int_distribution<int>(1, 100); // Define the range
    }

    virtual  int  next()
    {
        int  num = 1;

        while  ( coin_flip() )
            num = (num << 1) + coin_flip();

        return  num;
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
        if  ( duration()  >= time_limit )
            return  true;

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
    bool  f_done, f_verbose;
    string  success_file;
    AbstractSequence  * seq_gen;
    int  time_out, copy_time_out;

    int  time_counter;

    /// Wide search implementation
    bool  f_wide_search, f_parallel_search, f_random_search;

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
    int  g_timer, scale;

public:
    void  set_verbose( bool  _flag ) { f_verbose = _flag; }
    void  set_timeout( int  t ) { time_out = t; }
    void  set_copy_timeout( int  t ) { copy_time_out = t; }
    void  set_success_file( string s ) { success_file = s; }
    bool  is_found_success() {
        return  f_success_found;
    }
    void  set_seq_generator( AbstractSequence  * _seq_gen ) {
        seq_gen = _seq_gen;
    }

    void  set_wide_search( bool  flag ) { f_wide_search = flag; }
    void  set_random_search( bool  flag ) { f_random_search = flag; }
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
        f_random_search = f_parallel_search = f_wide_search = false;
        success_fn = NULL;
        f_done = false;
        counter_tasks_created = 0;
        f_verbose = f_success_found = false;
        max_jobs_number = 1;
        f_scheduler_stop = false;
        seq_gen = NULL;
        g_timer = 0;
        scale = 1;
        time_out = -1;
        copy_time_out = -1;
    }

    const char   * get_mode_str();
    bool  slots_available();
    void wakeup_thread() {
    }

    void set_success( bool  f_val ) {
        f_success_found = f_val;
    }

    void   set_scale( int  _scale ) { scale = _scale; }
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

    void  prepare_to_run();
    void  main_loop();

    void  suspend_process( int  ind );
    void  resume_first_process();

    void  verify_suspended_tasks_heap();
};

/*--- Start of code ---*/


void  SManager::check_for_success()
{
    //printf( "SUCCESS?\n" ); fflush( stdout );
    //printf( "\n\n\n\n\nSUCC: %s\n", get_success_file_name() );
    if  ( is_file_exists( get_success_file_name() ) )
        f_success_found = true;
}

////////////////////////////////////////////////////////////////////////////////
/// Copied shameless from ps source code...

#define PIDSITEMS  70
#define proc_t  struct pids_stack

struct pids_info *Pids_info = NULL;   // our required <pids> context
enum pids_item *Pids_items;           // allocated as PIDSITEMS
int Pids_index;                       // actual number of active enums
long Hertz;

void procps_init(void)
{
    Hertz = procps_hertz_get();
    proc_t *p;
    // --- <pids> interface --------------------------------------------------
    if (!Pids_items) {
        Pids_items = (pids_item *)calloc(PIDSITEMS, sizeof(enum pids_item));
        assert( Pids_items != NULL );
    }

    int  i;

    for (int  i = 0; i < PIDSITEMS; i++)
        Pids_items[i] = PIDS_noop;

    i = PIDSITEMS;
    if (!Pids_info) {
        if (procps_pids_new(&Pids_info, Pids_items, i)) {
            fprintf(stderr, "fatal library error, context\n");
            exit( -1 );
        }
    }

    Pids_items[0] = PIDS_TTY;
    procps_pids_reset(Pids_info, Pids_items, 1);
    if (!(p = fatal_proc_unmounted(Pids_info, 1))) {
        fprintf(stderr, "fatal library error, lookup self\n");
        exit( -1 );
    }
    // --- <pids> interface --------------------------------------------------
}

/* a 'results stack value' extractor macro
   where: E=rel enum, T=data type, S=stack */
#define rSv(E,T,S) PIDS_VAL(rel_ ## E, T, S, Pids_info)

#define namREL(e) rel_ ## e
//#define makEXT(e) extern int namREL(e);
#define makREL(e) int namREL(e) = -1;
#define chkREL(e) if (namREL(e) < 0) { \
      Pids_items[Pids_index] = PIDS_ ## e; \
      namREL(e) = (Pids_index < PIDSITEMS) ? Pids_index++ : rel_noop; }

#define makEXT(e)  int namREL(e);

makEXT(TIME_ALL)
makEXT(noop)
makEXT(TICS_ALL)
makEXT(TICS_ALL_C)
//makEXT(TICS_ALL)
//makEXT2(TICS_ALL_C)


#define setREL1(e) { \
  if (!outbuf) { \
    chkREL(e) \
    return 0; \
  } }
#define setREL2(e1,e2) { \
  if (!outbuf) { \
    chkREL(e1) chkREL(e2) \
    return 0; \
  } }


#define  STRLEN  1024

/* cumulative CPU time in seconds (not same as "etimes") */
int pr_times( char * outbuf,
                     proc_t * pp){
    //unsigned long long t;
    //unsigned long t;
    //setREL1(TIME_ALL)
    //   t = rSv(TIME_ALL, real, pp);

    unsigned long long tx;
    unsigned ux;
    setREL2(TICS_ALL,TICS_ALL_C)
        //        tx = rSv(TICS_ALL_C, ull_int, pp);
        tx = rSv(TICS_ALL_C, ull_int, pp);

    //printf( "TX: %llu\n", tx );
    ux = tx / Hertz;

    return snprintf(outbuf, STRLEN, "%3u:%02u", ux/60U, ux % 60U);
    
    //return snprintf(outbuf, STRLEN, "%lu", tx);
}


void  report_info( pid_t  pid )
{
    struct pids_fetch * pidread;
    unsigned pidlist[10];
    proc_t *buf;
    enum pids_select_type which = PIDS_SELECT_PID;
    
    pidlist[ 0 ] = pid;
    //printf( "PID: %d\n", pid ); fflush( stdout );
    pidread = procps_pids_select(Pids_info, pidlist, 1, which);
    if (!pidread) {
        fprintf(stderr, "fatal library error, reap\n" );
        exit( -1 );
    }

    int  i;
    for (i = 0; i < pidread->counts->total; i++) {
        buf = pidread->stacks[i];
        if(-1==(long)buf)
            continue;
        char  str[ STRLEN + 1 ];
        pr_times( str, buf );
        //printf( "PROCPS TIME: %s\n", str );
    }
        

}


void  SManager::check_for_done_tasks()
{
    check_for_success();
    for  ( int  ind  = active_tasks.size() - 1; ind >= 0; ind-- ) {
        Task  * p_task = active_tasks[ ind ];

        report_info( p_task->get_child_pid() );

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
        //kill_process( p_task->get_child_pid() );
        send_signal( p_task->get_child_pid(), SIGKILL, "SIGKILL" );
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
        //send_signal( cpid, SIGKILL, "SIGTERM" );

        // trap "echo received sigterm" SIGTERM

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
    printf( "## Task created: %d\n", counter_tasks_created );
    
    // We run a process for one second if we are in the wide search mode...
    if  ( f_wide_search ) {
        tsk->set_id( counter_tasks_created );
        tsk->set_time_limit( scale );
        tsk->set_time_delta( (int)seq_gen->next() );
        tsk->compute_next_wakeup();
        //printf( "ZZZZ\n" ); fflush( stdout );
    } else {
        int limit = scale * (int)seq_gen->next();
        printf( "LIMIT: %d\n", limit );
        tsk->set_time_limit( limit );
    }
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
    if  ( f_random_search )
        return "Random search";

    return   "Counter search";
}


void  SManager::prepare_to_run()
{
    char  out_success_fn[ 1024 ];

    std::filesystem::remove_all( work_dir.c_str() );
    mkdir( work_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH );

    char  * rpath = realpath( work_dir.c_str(), out_success_fn );
    assert( rpath != NULL );
    append_slash( out_success_fn );

    set_work_dir( out_success_fn );

    strcat( out_success_fn, "success.txt" );

    set_success_file( out_success_fn );
}



void   SManager::main_loop()
{
    printf( "MODE               : %s\n", get_mode_str() );
    printf( "# of parallel jobs : %d\n", max_jobs_number );
    printf( "Scale              : %d\n", scale );
    printf( "-----------------------------------------------------------\n\n" );
    fflush( stdout );

    time_t  start_time = time( NULL );

    //int  time_diff = 0;
    do {
        int  curr_time = (int)( time( NULL ) - start_time );
        printf( "Current time: %d\n", curr_time );
        fflush( stdout );

        if  ( ( time_out > 0 )  &&  ( curr_time > time_out ) ) {
            printf( "\n\n\n" "Timeout exceeded! Exiting... \n\n\n" );
            break;
        }

        if  ( f_verbose )
            printf( "Active: %4d   suspended: %4d   SIM TIME: %6d\n",
                    (int)active_tasks.size(),
                    (int)suspended_tasks.size(),
                    time_counter );

        check_for_done_tasks();
        if  ( is_found_success() )
            break;

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

////////////////////////////////////////////////////////////////////


void  usage()
{
    printf( "Catalyst\n\t"
            "Job scheduler for multi threaded system to avoid "
            "catastrophic running times for Las Vegas Algorithms.\n" );
    exit( -1 );
}





////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
/// Argument handling...

/* Program documentation. */
static char doc[] =
  "\n" "catalyst [Options]   PROG   WORKDIR";

/* A description of the arguments we accept. */
static char args_doc[] = "PROG   WORK-DIR";

const char *argp_program_version =
  "catalyst 0.4";
const char *argp_program_bug_address =
  "<sariel@illinois.edu>";


/* The options we understand. */
static struct argp_option options[] = {
    {"wide",     'a', 0,      0,  "Wide search" },
    {"parallel", 'p', 0,      0,  "Parallel search" },
    {"verbose",  'v', 0,      0,  "Verbose" },
    {"boring",   'b', 0,      0,  "Boring: Runs a single thread "
                                  "no fancy nonsense." },
    {"random",   'r', 0,      0,  "Random search" },
    {"gtimeout", 't', "Seconds", OPTION_ARG_OPTIONAL,
      "Timeout on running time of program"},
    {"gtimeout", 'f', "Seconds", OPTION_ARG_OPTIONAL,
      "Timeout on running time of a copy of alg."},
    {"scale",  's', "Seconds", OPTION_ARG_OPTIONAL,
      "Scale to use."},
  { 0 }
};


/* Used by main to communicate with parse_opt. */
struct ArgsInfo
{
    bool  f_wide_search, f_verbose, f_boring, f_random_search;
    bool  f_parallel_search;
    int   time_out, scale, copy_time_out;    
    const char *program;
    const char *work_dir;
    unsigned int  num_threads;

    void init() {
        /* Default values. */
        f_random_search = f_wide_search = false;
        f_boring = f_verbose = f_parallel_search = false;
        time_out = -1;
        copy_time_out = -1;
        program = "";
        work_dir = "";
        scale = 1;
        num_threads = std::thread::hardware_concurrency();
    }
};



/* Parse a single option. */
static error_t    parse_opt (int key, char *arg, struct argp_state *state)
{
    /* Get the input argument from argp_parse, which we
       know is a pointer to our arguments structure. */
    ArgsInfo    * p_info = (ArgsInfo *)state->input;

  assert( p_info != NULL );

  ArgsInfo  & info( *p_info );

  switch (key) {
  case 'a':
      info.f_wide_search = true;
      break;

  case 'r':
      info.f_random_search = true;
      break;

  case 'v':
      info.f_verbose = true;
      break;

  case 'b':
      info.f_boring = true;
      break;

  case 'p':
      info.f_parallel_search = true;
      break;

  case  't' :
      info.time_out = arg ? atoi(arg) : 100000;
      if   ( info.time_out < 1 ) {
          printf( "\n\n" "Error: Timeout has to be larger than 0!\n" );
          exit( -1 );
      }
      break;

  case  'f' :
      info.copy_time_out = arg ? atoi(arg) : 100000;
      if   ( info.copy_time_out < 1 ) {
          printf( "\n\n" "Error: Copy timeout has to be larger than 0!\n" );
          exit( -1 );
      }
      break;

  case  's' :
      info.scale = arg ? atoi(arg) : 1;
      if   ( info.scale < 1 ) {
          printf( "\n\n" "Error: Scale has to be larger than 0!\n" );
          exit( -1 );
      }
      break;

  case ARGP_KEY_NO_ARGS:
      argp_usage (state);


  case ARGP_KEY_ARG:
      if (state->arg_num >= 2)
        /* Too many arguments. */
        argp_usage (state);

      if  ( state->arg_num == 0 )
          info.program  = arg;
      if  ( state->arg_num == 1 )
          info.work_dir  = arg;
      break;

  case ARGP_KEY_END:
      if (state->arg_num < 2)
          /* Not enough arguments. */
          argp_usage (state);
      break;

  default:
      return ARGP_ERR_UNKNOWN;
  }

  if  ( info.f_wide_search  &&   info.f_parallel_search ) {
      printf( "\n\n" "Not allowed to do both parallel search and wide search!!!\n" );
      argp_usage (state);
  }

  return 0;
}

/* Our argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc };


void   parse_command_line( ArgsInfo  & info, int  argc, char  ** argv )
{
    /* Parse our arguments; every option seen by parse_opt will
       be reflected in arguments. */
    argp_parse (&argp, argc, argv, 0, 0, &info);
}



////////////////////////////////////////////////////////////////////

int  main(int   argc, char*   argv[])
{
    ArgsInfo  opt;

    opt.init();

    parse_command_line( opt, argc, argv );

    // Just terminate child processes when they are done, don't let
    // them become zombies...
    struct sigaction arg;
    arg.sa_handler=SIG_IGN;
    arg.sa_flags=SA_NOCLDWAIT; // Never wait for termination of a child process.

    sigaction(SIGCHLD, &arg, NULL);

    //SequenceCounter   seq_gen;
    SManagerPtr p_manager = new SManager();
    assert( p_manager != NULL );

    procps_init();

    //p_manager->set_threads_num( opt.num_threads );
    //p_manager->set_threads_num( (2 * opt.num_threads) / 3  );
    p_manager->set_threads_num( 12 );
    p_manager->set_program( opt.program );
    p_manager->set_scale( opt.scale );
    p_manager->set_work_dir( opt.work_dir );

    if   ( opt.f_boring ) {
        opt.f_parallel_search = true;
        p_manager->set_wide_search( false );
        p_manager->set_parallel_search( true );
        p_manager->set_threads_num( 1 );
    }

    if  ( ( opt.copy_time_out > 0 )  &&  ( !opt.f_parallel_search ) ) {
        printf( "Error: copy timeout larger than zero can be used only with "
                "parallel search.\n\n" );
        exit( -1 );
    }
        
    
    if  ( opt.f_wide_search ) {
        p_manager->set_wide_search( true );
        p_manager->set_seq_generator( new  SequencePlusOne() );
    } else if  ( opt.f_parallel_search ) {
        p_manager->set_parallel_search( true );
        if  ( opt.copy_time_out > 0 )
            p_manager->set_seq_generator( new
                       SequenceRepeatVal( opt.copy_time_out ) );
        else
            p_manager->set_seq_generator( new  SequenceMaxInt() );
    } else if  ( opt.f_random_search ) {
        p_manager->set_seq_generator( new  SequenceRandom() );
    } else {
        p_manager->set_seq_generator( new  SequenceCounter() );
    }
    p_manager->set_verbose( opt.f_verbose );

    p_manager->prepare_to_run();

    printf( "Work directory     : %s\n", p_manager->get_work_dir().c_str() );
    printf( "Success file       : %s\n", p_manager->get_success_file_name() );

    p_manager->set_timeout( opt.time_out );
    p_manager->set_copy_timeout( opt.copy_time_out );

    p_manager->main_loop();

    ///printf( "Everything is done???\n" );

    return  0;
}


/* catalyst.C - End of File ------------------------------------------*/
