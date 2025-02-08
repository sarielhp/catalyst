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
    printf( "usage:\n\twait_random_busy  [working-dir] [success_file]\n" );
    exit( -1 );
}


void  wait_one_second()
{
    long long   limit = (long long)( 1739130434 );
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

void  search_for_limit()
{
    long long   limit = 100000000;

    printf( "\n" "Computing what is the loop counter one has to set so that one"
            " second is wasted\n");
    int  k = 100;
    int  d = -1;
    int total = 0;

    while  ( d != 0 ) {
        printf( "Limit: %lld\n", limit ); fflush( stdout );
        d = wait_for_k_seconds_limit( k, limit );
        total = d + k;
        printf( "Diff: %d\n", d );

        if  ( d < 0 ) {
            double  ratio = (double)k / (double)total;
            //double  ratio_lb = 1.0 + 1.0 / (2.0*(double)k);

            limit = (long long)( ((double)limit) * ratio );
            continue;
        }
        if  ( d >= 1 ) {
            double  ratio = (double)k / (double)total;
            limit = (long long)( ((double)limit) * ratio );
            continue;
        }
    }
    printf( "LIMIT (for one second): %lld\n", limit );
}


int  main( int   argc, char   * argv[] )
{
    if  ( argc == 2 )
        search_for_limit();

    if  (argc != 3 )
        usage();


    std::random_device rd; // Hardware-based random number generator
    std::mt19937 generator(rd()); // Mersenne Twister engine seeded with rd()
    std::uniform_int_distribution<int> distribution(1, 100); // Define the range

    int  randomNum = 10 + distribution(generator);;

    //    randomNum = 1;
    printf( "ACTIVELY Waiting for %d seconds...\n", randomNum );

    time_t  start_time;//, end_time;
    start_time = time( NULL );
    //end_time = start_time + randomNum;

    for  ( int i = 0; i < randomNum; i++ ) {
        wait_one_second();
    }

    printf( "Diff: %d\n", (int)( time( NULL ) - start_time ) );


    //printf( "LIMIT: %lld\n", count );
    printf( "done\n" );

    string  fname( argv[ 2 ] );

    FILE * fl = fopen( fname.c_str(), "wt" );
    fclose( fl );

    return  0;
}

/*--- Constants ---*/
