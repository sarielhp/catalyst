/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
 * wait10.C -
 *
\*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/

#include  <stdlib.h>
#include  <stdio.h>
#include  <assert.h>

#include <unistd.h>

#include  <string>
using namespace std;

void  usage()
{
    printf( "usage:\n\twait10  [working-dir] [success-file]\n" );
    exit( -1 );
}


int  main( int   argc, char   * argv[] )
{
    if  (argc != 3 )
        usage();

    sleep(10);
    printf( "done\n" );


    FILE * fl = fopen( argv[2], "wt" );
    fclose( fl );

    return  0;
}


/*--- Constants ---*/
