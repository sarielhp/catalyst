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


int  main( int   argc, char   * argv[] )
{
    if  (argc != 3 )
        usage();


    std::random_device rd; // Hardware-based random number generator
    std::mt19937 generator(rd()); // Mersenne Twister engine seeded with rd()
    std::uniform_int_distribution<int> distribution(1, 100); // Define the range

    int  randomNum = 10 + distribution(generator);;

    printf( "ACTIVELY Waiting for %d seconds...\n", randomNum );

    time_t  start_time, end_time;
    start_time = time( NULL );
    end_time = start_time + randomNum;

    long long   count = 0;
    while  ( time( NULL ) <= end_time ) {
        count++;
    }

    printf( "done\n" );

    string  fname( argv[ 2 ] );

    FILE * fl = fopen( fname.c_str(), "wt" );
    fclose( fl );

    return  0;
}

/*--- Constants ---*/
