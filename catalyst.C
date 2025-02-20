/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
 * catalyst.C -
 *     A catastrophe avoiding scheduler.
 ---------------------------------------------------------------
 * Version 0.7, Sat Feb 15 02:32:06 PM CST 2025
 *     Total rewrite to have more accurate time tracking. Now
 *     sleeping the correct time in milliseconds.
\*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/

#include  <sys/stat.h>
#include  <stdlib.h>
#include  <stdio.h>
#include  <assert.h>
#include  <string.h>
#include  <unistd.h>
#include  <spawn.h>
#include  <sys/wait.h>
#include  <limits.h>
#include  <argp.h>

#include  <time.h>
#include  <stdarg.h>
#include  <signal.h>

#ifdef PROCPS_COMPILR
#include <libproc2/misc.h>
#endif

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

#ifdef PROCPS_COMPILR
#include <libproc2/pids.h>
#endif


/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

static bool   f_sched_fill_second = true;


/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////
/* Used by main to communicate with parse_opt. */

#define  MODE_NOT_DEF        0
#define  MODE_WIDE           1
#define  MODE_BORING         2
#define  MODE_RANDOM         3
#define  MODE_COUNTER        4
#define  MODE_PARALLEL       5
#define  MODE_ZETA_2         6


class WMode
{
private:
    int   mode;
public:
    WMode() { mode = 0; }

    int  get() const { return  mode; }
    void set( int  _val = 0 ) { mode = _val; }
    bool  is_boring() { return  mode == MODE_BORING; }
    bool  is( int  val ) { return  mode == val; }

};

struct ArgsInfo
{
    WMode  mode;
    bool  f_verbose;
    bool  f_print_succ_file;
    bool  f_jobs_cache;
    int   time_out, scale, copy_time_out;
    const char *program;
    const char *work_dir;
    unsigned int  num_threads;

    void init() {
        /* Default values. */
        f_print_succ_file = false;
        f_verbose = false;
        f_jobs_cache = false;
        time_out = -1;
        copy_time_out = -1;
        program = "";
        work_dir = "";
        scale = 1;
        num_threads = std::thread::hardware_concurrency();
        mode.set( MODE_NOT_DEF );
    }
};




/////////////////////////////////////////////////////////////
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



#define  PI_SQ_OVER_6  0.6079271018540267

// Basel distribution:
// Returns the value i with probability proportional to 1/i^2....
class   SequenceRBasel : public AbstractSequence
{
private:
    std::random_device rd; // Hardware-based random number generator
    std::uniform_real_distribution<>  distribution;
    std::mt19937 generator;

    double    get_real_sample() {
        return   distribution( generator ) ;
    }

public:
    SequenceRBasel()
    {
        generator = std::mt19937(rd()); // Mersenne Twister engine seeded with rd()
        distribution  =  std::uniform_real_distribution<>( 0.0, 1.0 );
    }

    virtual  int  next()
    {
        long double  val = get_real_sample();
        long double  sum;

        //val = 0.9999999;
        //printf( "val: %Lg\n", val );
        int  i = 1;
        sum = PI_SQ_OVER_6;
        while  ( sum < val ) {
            i = i + 1;
            long double ix = i;
            sum += PI_SQ_OVER_6 / ( ix  * ix  );
            //printf( "Diff: %Lg\n", val- sum );
        }
        if (  i == 1 )
            return  1;
        return  i;
    }
};


/////////////////////////////////////////////////////////////////////////////



inline bool   is_file_exists(const std::string& name) {
  struct stat   buffer;
  return (stat( name.c_str(), &buffer ) == 0);
}


/*--- Constants ---*/
typedef  pair<int, int>  PairInt;

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

    int  total_runtime, total_rounds;

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
        total_runtime = total_rounds = 0;
    }

    int   rounds() { return  total_rounds; }
    int   runtime() { return  total_runtime; }

    PairInt  profile() { return  PairInt( total_runtime, total_rounds ); }

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

    void  compute_next_wakeup( ) {
        assert( time_delta > 0 );
        //int x = next_wakeup;
        next_wakeup += time_delta;
        //printf( "MMM %d -> %d\n", x, next_wakeup );
    }

    void  inc_total_rounds() {
        total_rounds++;
    }

    void  update_total_runtime()
    {
        total_runtime += duration();
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
    int  time_out, copy_time_out, max_suspends, min_sus_runtime;

    int  time_counter;

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

    WMode  wmode, orig_wmode;

public:
    const WMode   & mode();
    bool  is( int _val ) { return  wmode.is( _val ); }
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

    void  print_profile();

    /*
    void  set_wide_search( bool  flag ) { f_wide_search = flag; }
    void  set_random_search( bool  flag ) { f_random_search = flag; }
    void  set_parallel_search( bool  flag ) { f_parallel_search = flag; }
    `*/

    void  kill_min_suspended_task();
    void  update_min_suspended_runtime();
    void  resume_sus_process( int  pos, int  new_time_limit );
    int   get_suspended_task_to_wake( int  time_limit );

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
        //f_random_search = f_parallel_search = f_wide_search = false;
        success_fn = NULL;
        f_done = false;
        counter_tasks_created = 0;
        f_verbose = f_success_found = false;
        max_jobs_number = 1;
        max_suspends = 0;
        min_sus_runtime = 0;

        f_scheduler_stop = false;
        seq_gen = NULL;
        g_timer = 0;
        scale = 1;
        time_out = -1;
        copy_time_out = -1;
    }

    void  set_orig_mode( WMode  &_mode ) { orig_wmode = _mode; }
    void  set_mode( WMode  &_mode ) { wmode = _mode; }

    const char   * get_mode_str();
    bool  slots_available();
    void wakeup_thread() {
    }

    void set_success( bool  f_val ) { f_success_found = f_val; }

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
    void  set_threads_num( int  n ) {  max_jobs_number = n; }
    void  set_max_suspends_num( int  n ) { max_suspends = n; }
    void  spawn_single_task();
    void  spawn_tasks();
    void  check_for_done_tasks();

    void  prepare_to_run();
    void  main_loop();

    void  suspend_process( int  ind );
    void  resume_first_process();

    void  verify_suspended_tasks_heap();
    void  setup( ArgsInfo  & opt );
};

/*--- Start of code ---*/
void  SManager::setup( ArgsInfo  & opt )
{
    set_scale( opt.scale );
    set_work_dir( opt.work_dir );

    set_orig_mode( opt.mode );
    set_mode( opt.mode );

    
    //printf( "MODE = %d\n",  opt.mode.get() );
    if  ( is( MODE_NOT_DEF ) ) {
        printf( "You must select a search mode...\n" );
    }

    set_verbose( opt.f_verbose );
    set_timeout( opt.time_out );
    set_copy_timeout( opt.copy_time_out );

    if   ( wmode.is_boring() ) {
        wmode.set( MODE_PARALLEL );
        set_threads_num( 1 );
    }
    if  ( ( opt.copy_time_out > 0 )  &&  ( ! wmode.is( MODE_PARALLEL ) ) ) {
        printf( "Error: copy timeout larger than zero can be used only with "
                "parallel search.\n\n" );
        exit( -1 );
    }
    if  ( is( MODE_WIDE ) ) {
        //set_wide_search( true );
        set_seq_generator( new  SequencePlusOne() );
        return;
    }
    if  ( is( MODE_PARALLEL ) ) {
        if  ( opt.copy_time_out > 0 )
            set_seq_generator( new SequenceRepeatVal( opt.copy_time_out ) );
        else
            set_seq_generator( new  SequenceMaxInt() );
        return;
    }
    if  ( is( MODE_RANDOM ) ) {
        set_seq_generator( new  SequenceRandom() );
            return;
    }
    if  ( is( MODE_ZETA_2 ) ) {
        set_seq_generator( new    SequenceRBasel() );
        return;
    }
    if  ( is( MODE_COUNTER ) ) {
        set_seq_generator( new    SequenceCounter() );
        return;
    }
    printf( "ERROR: Must specify a mode!\n" );
    exit ( -1 );
}


void  SManager::check_for_success()
{
    //printf( "SUCCESS?\n" ); fflush( stdout );
    //printf( "\n\n\n\n\nSUCC: %s\n", get_success_file_name() );
    if  ( is_file_exists( get_success_file_name() ) )
        f_success_found = true;
}

////////////////////////////////////////////////////////////////////////////////
/// Copied shameless from ps source code...

#ifdef PROCPS_COMPILR

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
#endif  // PROCPS


void  SManager::check_for_done_tasks()
{
    check_for_success();
    for  ( int  ind  = active_tasks.size() - 1; ind >= 0; ind-- ) {
        Task  * p_task = active_tasks[ ind ];

        #ifdef PROCPS_COMPILE
        report_info( p_task->get_child_pid() );
        #endif

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
    //printf( "Before...\n" );
    for ( unsigned  k = 0; k < ( suspended_tasks.size() - 1 ); k++ ) {
        unsigned u = 2*k+1;
        if  ( u >= suspended_tasks.size() )
            break;
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
    //printf( "After...\n" ); fflush( stdout );
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

    if  ( is( MODE_WIDE ) )
        p_task->compute_next_wakeup();

    suspended_tasks.push_back( p_task );
    push_heap( suspended_tasks.begin(), suspended_tasks.end(),
               TaskComparatorByWakeup() );

    // Verify suspended tasks are sorted correct in the *heap*
    verify_suspended_tasks_heap();
    if  ( f_verbose ) {
        printf( "Suspended tasks size: %4d  A:%4d  TIMER: %6d\n",
                (int)suspended_tasks.size(),
                (int)active_tasks.size(), time_counter );
        fflush( stdout );
    }
}



void   SManager::resume_first_process()
{
    assert( is( MODE_WIDE ) );
    assert( suspended_tasks.size() > 0 );

    Task * p_task = suspended_tasks.front() ;

    // Update the start time of the process
    p_task->record_start_time();
    p_task->inc_total_rounds();

    //printf( "About to send signal...\n" ); fflush(stdout );
    //----------------------------------------------------
    // Suspending the process: Low level stuff
    send_signal( p_task->get_child_pid(), SIGCONT, "SIGCONT" );
    //printf( "After  sending signal...\n" ); fflush(stdout );

    /// Moving it from suspend to resumed tasks...
    active_tasks.push_back( p_task );
    //printf( "XAfter  sending signal...\n" ); fflush(stdout );

    pop_heap( suspended_tasks.begin(), suspended_tasks.end(),
              TaskComparatorByWakeup() );
    //printf( "YXAfter  sending signal...\n" ); fflush(stdout );
    suspended_tasks.pop_back();
    //printf( "ZZYXAfter  sending signal...\n" ); fflush(stdout );
    verify_suspended_tasks_heap();
    //printf( "___ZXAfter  sending signal...\n" ); fflush(stdout );
}





void   SManager::resume_sus_process( int  pos, int  new_time_limit )
{
    assert( max_suspends > 0 );
    assert( (int)suspended_tasks.size() > pos );

    Task * p_task = suspended_tasks[ pos ];

    // Update the start time of the process
    p_task->record_start_time();
    p_task->set_time_limit( new_time_limit - p_task->runtime() );
    p_task->inc_total_rounds();

    //----------------------------------------------------
    // Suspending the process: Low level stuff
    send_signal( p_task->get_child_pid(), SIGCONT, "SIGCONT" );

    /// Moving it from suspend to resumed tasks...
    active_tasks.push_back( p_task );
    suspended_tasks.erase( suspended_tasks.begin() + pos );
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

void   SManager::kill_min_suspended_task()
{

    assert( max_suspends > 0 );

    if  ( suspended_tasks.size() == 0 )
        return;
    auto min_pos = 0;
    auto min_val = suspended_tasks[ 0 ]->runtime();
    for  ( int  i = 1; i < (int)suspended_tasks.size(); i++ ) {
        auto new_val = suspended_tasks[ i ]->runtime();
        if  ( new_val < min_val )  {
            min_val = new_val;
            min_pos = i;
        }
    }

    Task  * p_task = suspended_tasks[ min_pos ];
    pid_t   cpid = p_task->get_child_pid();

    /// First resume it...
    /// ... then kill it.
    send_signal( cpid, SIGCONT, "SIGCONT" );
    send_signal( cpid, SIGKILL, "SIGKILL" );

    //kill_process( p_task->get_child_pid() );
    suspended_tasks.erase( suspended_tasks.begin() + min_pos );
}


void     SManager::update_min_suspended_runtime()
{
    if  ( (int)suspended_tasks.size() < ( max_suspends - 1 ) )
        return;

    auto min_val = suspended_tasks[ 0 ]->runtime();
    for  ( int  i = 1; i < (int)suspended_tasks.size(); i++ ) {
        auto new_val = suspended_tasks[ i ]->runtime();
        if  ( new_val < min_val )
            min_val = new_val;
    }
    if  ( min_val > min_sus_runtime )
        min_sus_runtime = 1+min_val;
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
        //printf( "Expired task????\n" ); fflush( stdout );

        check_for_success();

        if  ( p_task->is_successful() )
            f_success_found = true;

        pid_t cpid = p_task->get_child_pid();
        if   ( f_success_found
               ||  ( ( ! is( MODE_WIDE ) )  &&  ( max_suspends == 0 ) ) ) {
            count++;
            kill_process( cpid );
            p_task->set_done( true );
            continue;
        }
        if   ( is( MODE_PARALLEL ) )
            continue;

        // f_success_found == false  &&  f_wide_search = true
        assert( ! f_success_found );
        assert( p_task->is_expired() );
        assert( is( MODE_WIDE )  ||  ( max_suspends > 0 ) );

        p_task->update_total_runtime();
        if  ( is( MODE_WIDE ) ) {
            suspend_process( ind );
            continue;
        }

        /// Unimportant - should be killed...
        if  ( ( ! is( MODE_WIDE )  )
              &&   ( p_task->runtime() <= min_sus_runtime ) ) {
            kill_process( cpid );
            p_task->set_done( true );
            continue;
        }

        // A combined search. Should we kill it, or should we suspend it?
        if  ( (int)suspended_tasks.size() < (int)max_suspends ) {
            suspend_process( ind );
            update_min_suspended_runtime();
            continue;
        }

        kill_min_suspended_task();
        suspend_process( ind );
        update_min_suspended_runtime();
    }
    if  ( ( count > 0 )   &&   ( f_verbose ) ) {
        printf( "Killed %d processes\n", count );
        fflush( stdout );
    }
}


void  SManager::print_profile()
{
    vector<PairInt>  totals;

    for  ( int  ind  = active_tasks.size() - 1; ind >= 0; ind-- ) {
        totals.push_back( active_tasks[ ind ]->profile() );
   }
    for  ( int  ind  = suspended_tasks.size() - 1; ind >= 0; ind-- ) {
        totals.push_back( suspended_tasks[ ind ]->profile() );
    }
    sort(totals.begin(), totals.end(), greater<>());

    for ( int  i = 0; i < (int)totals.size(); i++ ) {
        printf( " %3d: %3d [Rounds: %d]\n", i, totals[ i ].first,
                totals[ i ].second );
    }
    fflush( stdout );

}



int   SManager::get_suspended_task_to_wake( int  time_limit )
{
    int   pos = -1;
    int   rt = -1;
    for ( unsigned  k = 0; k < suspended_tasks.size(); k++ ) {
        auto p_task = suspended_tasks[ k ];
        if  ( p_task->runtime() >= time_limit )
            continue;
        if  ( p_task->runtime() > rt ) {
            rt = p_task->runtime();
            pos = k;
        }
    }
    //printf( "RRRR POS: %3d:  %3d -> %3d\n", pos, rt, time_limit );
    return  pos;
}


void  SManager::spawn_single_task()
{
    if  ( f_success_found )
        return;

    if  ( (int)( active_tasks.size() ) >= max_jobs_number )
        return;


    int  time_limit = scale * (int)seq_gen->next();
    //printf( "TTTTTT: %d\n", time_limit );

    if   ( ( max_suspends > 0 )  &&  ( ! is( MODE_WIDE ) ) ) {
        int  pos = get_suspended_task_to_wake( time_limit );
        if  ( pos >= 0 ) {
            resume_sus_process( pos, time_limit );
            return;
        }
    }


    Task  * tsk = new  Task();

    counter_tasks_created++;

    static int  max_tasks_c = 32;
    if  ( counter_tasks_created >= max_tasks_c ) {
        printf( "## Task created: %d\n", counter_tasks_created );
        max_tasks_c += counter_tasks_created;
    }
    // We run a process for one second if we are in the wide search mode...
    if  ( is( MODE_WIDE ) ) {
        tsk->set_id( counter_tasks_created );
        tsk->set_time_limit( scale );
        tsk->set_time_delta( time_limit );
        //printf( "DELTA: %d\n", time_limit );
        tsk->compute_next_wakeup();
        //printf( "ZZZZ\n" ); fflush( stdout );
    } else {
        //printf( "LIMIT: %d\n", time_limit );
        tsk->set_time_limit( time_limit );
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
    if  ( ! is( MODE_WIDE ) ) {
        while  ( slots_available() ) {
            spawn_single_task();
        }
        return;
    }

    assert( is( MODE_WIDE ) ); // Just for clarity

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

        //int tasks_alive;

        //tasks_alive = (int)suspended_tasks.size() + (int)active_tasks.size();

        int  future = time_counter + 1;
        //printf( "tasks : %d  < %d\n", tasks_alive, future );
        //for  ( int  k = 0; k <(int)suspended_tasks.size(); k++ ) {
        //  printf( " %d ", suspended_tasks[ k ]->next_wakeup_time() );
        //  }
        //printf( "\n" );fflush( stdout );

        if  ( suspended_tasks[ 0 ]->next_wakeup_time() < future ) {
            //printf( "resuming?\n" ); fflush( stdout );
            resume_first_process();
            //printf( "...resuming?\n" ); fflush( stdout );
            continue;
        }

        // Everything is in the future, we might as well move into it...
        time_counter++;   // TIME forward by one unit
        spawn_single_task();
    }
    //printf( "escape\n" );fflush( stdout );
}


const char   * SManager::get_mode_str()
{
    if  ( orig_wmode.is( MODE_BORING ) )
        return  "Boring search";
    if  ( is( MODE_WIDE ) )
        return  "Wide search";
    if  ( is( MODE_PARALLEL ) )
        return "Parallel search";
    if  ( is( MODE_RANDOM ) )
        return "Random search";
    if  ( is( MODE_ZETA_2 ) )
        return "Random Zeta_2 search";
    if  ( is( MODE_COUNTER ) )
        return "Counter search";

    assert( false );
    return   "";
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

typedef std::chrono::milliseconds  Milliseconds;
//Milliseconds  duration_ms( auto  x ) {
//    return  std::chrono::duration_cast<Milliseconds>( x );
//}
#define duration_ms std::chrono::duration_cast<Milliseconds>


void   SManager::main_loop()
{
    string smode = get_mode_str();
    if  ( max_suspends > 0 ) {
        smode = smode + " + Cache";
    }
    printf( "# MODE                     : %s\n", smode.c_str() );
    printf( "# Scheduler fill to second : %d\n", (int)f_sched_fill_second );

    printf( "# of parallel jobs     : %d\n", max_jobs_number );
    if  ( max_suspends > 0 )
        printf( "# max # suspended jobs : %d\n", max_suspends );
    printf( "# Time scale           : %d\n", scale );
    if  ( is( MODE_ZETA_2 ) )
        printf( "## Basel distribution  (Prob[X=i] ≈ 1/i^2)\n" );
    printf( "-----------------------------------------------------------\n\n" );
    fflush( stdout );

    time_t  start_time = time( NULL );

    auto t_start_global = std::chrono::steady_clock::now();
    do {
        auto t_start = std::chrono::steady_clock::now();
        int  curr_time = (int)( time( NULL ) - start_time );

        int  sus_count = (int)suspended_tasks.size();
        if  ( sus_count > 0 )
            printf( "Suspended tasks: %4d   ", sus_count );
        if  ( time_counter == 0 )
            printf( "Active: %5d  Real time: %6d\n",
                    (int)active_tasks.size(), curr_time );
        else
            printf( "Stats Tasks. Suspended: %4d Active: %5d     V"
                    "tm : %6d   Real time: %6d\n",
                    (int)suspended_tasks.size(),
                    (int)active_tasks.size(), time_counter, curr_time );
        fflush( stdout );
        //print_profile();

        //printf( "AX\n" ); fflush(stdout);
        if  ( ( curr_time & 0xf ) == 0 ) {
            fflush( stdout );
        }

        //printf( "AB\n" ); fflush(stdout);
        if  ( ( time_out > 0 )  &&  ( curr_time > time_out ) ) {
            printf( "\n\n\n" "Timeout exceeded! Exiting... \n\n\n" );
            break;
        }

        //printf( "AC\n" ); fflush(stdout);
        if  ( f_verbose )
            printf( "Active: %4d   suspended: %4d   SIM TIME: %6d\n",
                    (int)active_tasks.size(),
                    (int)suspended_tasks.size(),
                    time_counter );

        //        printf( "A\n" ); fflush(stdout);
        check_for_done_tasks();
        if  ( is_found_success() )
            break;

        //printf( "B\n" ); fflush(stdout);
        handle_expired_tasks();
        if  ( is_found_success() )
            break;
        //printf( "C\n" ); fflush(stdout);

        //printf( "ML C Looping...\n" );
        spawn_tasks();
        //printf( "C\n" ); fflush(stdout);
        //printf( "D Looping...\n" );
        if  ( is_found_success() )
            break;

        auto now = std::chrono::steady_clock::now();


        //namespace duration_cast =  std::chrono::duration_cast<Milliseconds>();
        auto elapsed = duration_ms( now - t_start );
        auto elapsed_global = duration_ms( now - t_start_global );


        int dur = elapsed.count();
        //int dur_global = elapsed_global.count();
        //printf( "Round time 1/1000 seconds: %d  [%d]\n",
        //        (int)dur, (int)dur_global );
        int sleep_micro = 1000*(1000-dur);

        /// Or lets just be stupid and sleep for a second...
        if  ( ! f_sched_fill_second )
            sleep_micro = 1000000;

        if  ( sleep_micro  > 0 )
            usleep( sleep_micro  );

        //printf( "...Waking\n" );
        //fflush( stdout );

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
    inc_total_rounds();

    if  ( p_manager->is_found_success() ) {
        f_done = true;
        return;
    }

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
    static int  count_out = 0;
    count_out++;
    if  ( count_out <= 10 )
        printf( "## PROCESS SPAWN  : %s\n", command.c_str() );

    fflush(  stdout  );

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


#define  VERSION  "0.7"
#define  PROGRAM  "Catalyst"

const char *argp_program_version = NULL;
const char *argp_program_bug_address =
  "<sariel@illinois.edu>";


#define  PRINT_SUCC_FILE  -1

/* The options we understand. */
static struct argp_option options[] = {
    {"counter",     'n', 0,      0,  "Counter search" },
    {"wide",        'a', 0,      0,  "Wide search" },
    {"parallel",    'p', 0,      0,  "Parallel search" },
    {"verbose",     'v', 0,      0,  "Verbose" },
    {"boring",      'b', 0,      0,  "Boring: Runs a single thread"
                                  "no fancy nonsense." },
    {"random",      'r', 0,      0,  "Random search" },
    {"cache",       'm', 0,      0,  "Use cache of suspended threads" },
    {"rbasel",      'R', 0,      0,  "Random search using Basel distribution" },
    {"gtimeout",    't', "Secs", OPTION_ARG_OPTIONAL,
      "Timeout on OVERALL running time of simulation."},
    {"copytimeout", 'c', "Secs", OPTION_ARG_OPTIONAL,
      "Timeout on running time of each COPY of alg."},
    {"scale",  's', "Seconds", OPTION_ARG_OPTIONAL,
      "Scale to use."},
    {"success_file", PRINT_SUCC_FILE, 0,      0,  "Output success file name." },
  { 0 }
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
  case  PRINT_SUCC_FILE:
      info.f_print_succ_file = true;
      break;

  case 'a':
      info.mode.set( MODE_WIDE );
      break;

  case 'n':
      info.mode.set( MODE_COUNTER );
      break;

  case 'r':
      info.mode.set( MODE_RANDOM );
      break;

  case 'm':
      info.f_jobs_cache = true;
      break;

  case 'R':
      info.mode.set( MODE_ZETA_2 );
      break;

  case 'v':
      info.f_verbose = true;
      break;

  case 'b':
      info.mode.set( MODE_BORING );
      break;

  case 'p':
      info.mode.set( MODE_PARALLEL );
      break;

  case  't' :
      info.time_out = arg ? atoi(arg) : 100000;
      if   ( info.time_out < 1 ) {
          printf( "\n\n" "Error: Timeout has to be larger than 0!\n" );
          exit( -1 );
      }
      break;

  case  'c' :
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
string  command_line( int   argc, char*   argv[] )
{
    string  s = "";

    for ( auto i = 0; i < argc; i++ ) {
        if  ( i > 0 )
            s = s + " ";
        s = s + argv[ i ];
    }

    return  s;
}


void  error( const char  * err )
{
    printf( "\n\n" "Error: [%s]\n\n", err );
    fprintf( stderr, "\n\n" "Error: [%s]\n\n", err );
    exit( -1 );
}


int  main(int   argc, char*   argv[])
{
    string prog_ver = string(  PROGRAM ) + " " + VERSION;

    argp_program_version = prog_ver.c_str();



    ArgsInfo  opt;

    /*
    SequenceRBasel  sq;

    for  ( int i = 0; i < 1; i++ ) {
        printf( "i: %d   : %d\n", i, sq.next() );
    }
    exit( -1 );
    */

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

#ifdef PROCPS_COMPILE
    procps_init();
#endif

    p_manager->set_threads_num( (3 * opt.num_threads) / 4 );
    if  ( opt.f_jobs_cache  )
        p_manager->set_max_suspends_num( opt.num_threads );

    p_manager->set_program( opt.program );

    if  ( ! is_file_exists( opt.program ) ) {
        fprintf( stderr, "\n\n\n" "Error: Program not found: %s\n\n\n", opt.program );
        exit( -1 );
    }

    p_manager->setup( opt );
    p_manager->prepare_to_run();

    string cmd = command_line( argc, argv );

    if  ( opt.f_print_succ_file ) {
        printf( "%s\n", p_manager->get_success_file_name() );
        return  0;
    }

    printf( "### %s\n###\n", cmd.c_str() );

    printf( "##### Catalyst %s\n", prog_ver.c_str() );
    printf( "# Work directory   : %s\n", p_manager->get_work_dir().c_str() );
    printf( "# Success file     : %s\n", p_manager->get_success_file_name() );

    p_manager->main_loop();

    return  0;
}


/* catalyst.C - End of File ------------------------------------------*/
