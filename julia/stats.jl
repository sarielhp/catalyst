#! /bin/julia
using DelimitedFiles
using Statistics

function  (@main)(ARGS)
    println( ARGS[ 1 ],"\n\n\n" );
    arr = open( ARGS[1] ) do f
        readlines(f) |> (s->parse.(Float64, s))
    end
    println( arr );
    println( length(arr) );
    println( typeof( arr ) );
    println( "Mean     : ", mean( arr ) );
    println( "Median   : ", median( arr ) );
    println( "StdDev   : ", Statistics.std( arr ) );
    println( "Variance : ", Statistics.var( arr ) );
end
