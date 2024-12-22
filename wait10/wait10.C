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
    printf( "usage:\n\twait10  [working-dir]\n" );
    exit( -1 );
}


int  main( int   argc, char   * argv[] )
{
    if  (argc != 2 )
        usage();

    sleep(10);
    printf( "done\n" );

    string  fname( string( argv[ 1 ] ) + string( "/success.txt" ) );

    FILE * fl = fopen( fname.c_str(), "wt" );
    fclose( fl );

    return  0;
}


/*--- Constants ---*/
