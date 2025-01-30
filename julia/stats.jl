#! /bin/julia
using DelimitedFiles
using Statistics

function  (@main)(ARGS)
    arr = open( ARGS[1] ) do f
        readlines(f) |> (s->parse.(Float64, s))
    end
    if length( arr ) == 0
        exit( -1 );
    end
#    println( arr );
    println( "" );
    println( "Filename : ", ARGS[ 1 ] );
    println( "#        : ", length(arr) );
#    println( typeof( arr ) );
    println( "Mean     : ", mean( arr ) );
    if  ( length( arr ) > 1 ) 
        println( "Median   : ", median( arr ) );
        println( "StdDev   : ", Statistics.std( arr ) );
    end
    println( "Minimum  : ", minimum( arr ) );
    println( "Maximum  : ", maximum( arr ) );
#    println( "Variance : ", Statistics.var( arr ) );
end
