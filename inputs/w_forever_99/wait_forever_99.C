/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
 * wait10.C -
 *
\*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/

#include  <stdlib.h>
#include  <stdio.h>
#include  <assert.h>
#include  <random>

#include <time.h>
#include <unistd.h>

#include <chrono>
#include  <string>
using namespace std;


void  usage()
{
    printf( "usage:\n\twait_forever_99  [working-dir] [success_file]\n" );
    exit( -1 );
}


#define  LIMIT 1739130434

void  wait_one_second()
{
    long long   limit = (long long)( LIMIT );
    long long   count = 0;
    for  ( long long i = 0; i < limit; i++ ) {
        count++;
    }
}

int   wait_for_k_seconds_limit( int  k, long long  limit )
{
    long long   count = 0;

    time_t start_time = time( NULL );
    for  ( int j = 0; j < k; j++ ) {
        for  ( long long  i = 0; i < limit; i++ ) {
            count++;
        }
    }
    return   (int)( time( NULL ) - start_time ) - k;
}


int  main( int   argc, char   * argv[] )
{
    if  (argc != 3 )
        usage();


    std::random_device rd; // Hardware-based random number generator
    std::mt19937 generator(rd()); // Mersenne Twister engine seeded with rd()
    std::uniform_int_distribution<int> distribution(1, 100); // Define the range

    wait_one_second();
    wait_one_second();
    wait_one_second();

    int  randomNum = 0;
    randomNum = distribution(generator);;

    if  (randomNum > 10 ) {
        printf( "Long wait....\n" );
        wait_for_k_seconds_limit( 999999, 1739130434 );
    }

    printf( "done\n" );

    string  fname( argv[ 2 ] );

    FILE * fl = fopen( fname.c_str(), "wt" );
    fclose( fl );

    return  0;
}

/*--- Constants ---*/
