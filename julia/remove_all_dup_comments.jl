#! /bin/julia
using DelimitedFiles
#using Statistics

function  strip_dup_comments( filename )
    lines = readlines( filename )
    out = Vector{String}();
    comments = Vector{String}();

    last_comment = " # bla bla";
    for  line ∈ lines
        line = strip( line );
        if  length( line ) == 0  continue  end;
        if  line[ 1 ] != '#'
            push!( out, line );
            continue;
        end

        pos = findfirst(x -> ( x == line ), comments );
        if  ( pos == nothing )
            push!( out, line );
            push!( comments, line );
            continue;
        end

        #println( "POS: ", line );
        #println( "comments[", pos, "] : ", comments[ pos ] );
        
    end

    writedlm( filename, out, "\n")
    #lines = readlines( filename )
    #println( v );
    return  out;
end

function  (@main)(ARGS)
    for  arg ∈ ARGS
        strip_dup_comments( arg );
    end
end
