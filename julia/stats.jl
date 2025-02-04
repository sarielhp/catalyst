#! /bin/julia
using DelimitedFiles
using Statistics

function  read_file_w_comments_floats( fl::String )
    if  ( ! isfile( fl ) )
        println( "\n\nError: File [", fl, "] does not exist?\n\n" );
        exit( -1 );
    end
    lines = readlines( fl )

    arr = Vector{Float64}();

    for  line ∈ lines
        ls = strip( line );
        if  ( length( ls ) == 0 ) continue end;
        if  ( ls[1] == '#' )   continue end;

        push!( arr, parse.(Float64, ls) );
    end

    return  arr;
end

function  remove_above( arr, timeout )
    len = length( arr );
    filter!( e -> e <= timeout, arr );
    return  len - length( arr );
end

function  stats_file( fl::String )
#    println( "FL: ", fl );
    arr = read_file_w_comments_floats( fl )

    number_timeouts = remove_above( arr, 3000 );
    if  ( length( arr ) == 0 )  return  end



    println( "" );
    println( "Filename     : ", ARGS[ 1 ] );
    println( "#            : ", length(arr) + number_timeouts );
    if  ( number_timeouts > 0 )
        println( "# timeouts   : ", number_timeouts );
        println( "# successes  : ", length(arr)  );
    end
    
    #    println( typeof( arr ) );
    println( "Mean         : ", mean( arr ) );
    if  ( length( arr ) > 1 )
        println( "Median       : ", median( arr ) );
        println( "StdDev       : ", Statistics.std( arr ) );
    end
    println( "Minimum      : ", minimum( arr ) );
    println( "Maximum      : ", maximum( arr ) );
end

function  (@main)(ARGS)
    for  arg ∈ ARGS
        stats_file( arg );
    end

    return  0;
end
