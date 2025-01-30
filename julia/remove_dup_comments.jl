#! /bin/julia
using DelimitedFiles
#using Statistics

function  strip_dup_comments( filename )
    lines = readlines( filename )
    out = Vector{String}();

    last_comment = " # bla bla";
    for  line ∈ lines
        line = strip( line );
        if  length( line ) == 0  continue  end;
        if  line[ 1 ] == '#'
            #println( "COMMENT: [", line, "]" );
            if  cmp( line, last_comment ) == 0
                continue;
            end
            last_comment = line;
        end

        push!( out, line );
    end

    writedlm(filename, out, "\n")
    #lines = readlines( filename )
    #println( v );
    return  out;
end

function  (@main)(ARGS)
    for  arg ∈ ARGS
        strip_dup_comments( arg );
    end
end
