#! /bin/julia
using DelimitedFiles
using Statistics

function  (@main)(ARGS)
    lines = readlines( ARGS[ 1 ])

    arr = Vector{Float64}();

    for  line ∈ lines
        ls = strip( lien );
        if  ( length( ls ) == 0 )
            continue;
        end
        if  ( ls.first == '#' )
            continue;
        end

        arr.push!( ls->parse.(Float64, s) );
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
