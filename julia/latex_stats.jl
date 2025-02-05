#! /bin/julia

using TimerOutputs
using Printf
using CSV, DataFrames
using PrettyTables
using LaTeXStrings


function  df_add_row( df::DataFrame )
    push!(df,["" for i in 1:ncol(df)], promote=true)
end

function  add_col( df::DataFrame, strs... )
    for  x in strs
        df[ :, x ] = String[];
    end
end


function  get_output_table( filename::String )
    local df;
    if  ( isfile( filename ) )
        return   CSV.read( filename, DataFrame, types=String );
    end


    #CL_APRX_2,
    add_col( df, CL_INDEX, CL_INPUT_P, CL_INPUT_Q,
             CL_APRX_4,
             CL_APRX_1_1,
             CL_APRX_1_01,
             CL_APRX_1_001,
             CL_EXACT,
             CL_VE_RETRACT, CL_DESC );

    return  df;
end


function  write_latex_table( out_name, df::DataFrame )
    ha = LatexHighlighter((d,i,j)->i % 2 != 0,
                          ["cellcolor{lightgray}","texttt"])
    hb = LatexHighlighter((d,i,j)->i % 2 == 0,
                          ["texttt"])

    iox = open( out_name, "w" );
    pretty_table( iox, df, header = names( df ), backend = Val(:latex),
                  highlighters = (ha, hb));

    close( iox );
end


using DelimitedFiles
using Statistics

function  sfloat( num::Float64 )
    s = @sprintf( "%.2f", num );
    return  s;
end

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

# Stolen from the internet...
function longest_common_prefix(strs::Vector{String})::String
    s1, s2 = minimum(strs), maximum(strs)
    pos = findfirst(i -> s1[i] != s2[i], 1:length(s1))
    return isnothing(pos) ? s1 : s1[1:(pos - 1)]
end

# Not efficient, but fun...
function longest_common_suffix(strs::Vector{String})::String
    revs = Vector{String}();
    for  s ∈ strs
        push!( revs, reverse( s ) )
    end
    suff = longest_common_prefix( revs );
    return  reverse( suff );
end

function  remove_above( arr, timeout )
    len = length( arr );
    filter!( e -> e <= timeout, arr );
    return  len - length( arr );
end

function  is_boring_column( df, column )
    runs_num = length( unique( sort( df[ :, column ] ) ) );
    return  ( runs_num == 1 );
end
function  rm_boring_column( df, column )
#    println( "RM ", column );
#    println( df );
    if  ( is_boring_column( df, column ) )
        select!(df, Not([column]))
    end
end

function  (@main)(ARGS)

    if  length( ARGS ) == 0
        println( "latex_table [run_1.log] ... [run_t.log]\n" );
        exit( -1 );
    end

    df = DataFrame();

    CL_SIMULATION = "Simulation";
    CL_RUNS = "Runs";
    CL_SUCC_RUNS = "# Succ";
    CL_FAIL_RUNS = "# Fails";
    CL_MEAN = "Mean";
    CL_MEDIAN = "Median";
    CL_STDDEV = "Std Dev";
    CL_MIN = "Min";
    CL_MAX = "Max";

    add_col( df, CL_SIMULATION)
    add_col( df, CL_RUNS )
    add_col( df, CL_SUCC_RUNS, CL_FAIL_RUNS );
    add_col( df,  CL_MEAN, CL_MEDIAN, CL_STDDEV,
        CL_MIN, CL_MAX );

    for  s ∈ ARGS
        arr = read_file_w_comments_floats( s )
        failures = remove_above( arr, 3000 );

        df_add_row( df );

        i = nrow( df );
        df[ i, CL_SIMULATION ] = s;
        df[ i, CL_RUNS       ] = string( length( arr ) + failures );
        df[ i, CL_SUCC_RUNS  ] = string( length( arr ) );
        if  ( failures > 0 )
            df[ i, CL_FAIL_RUNS  ] = L"\TFailX{$failures}";
        else
            df[ i, CL_FAIL_RUNS  ] = string( failures );
        end
        df[ i, CL_RUNS       ]  = string( length( arr ) + failures );
        df[ i, CL_MEAN       ] = sfloat( mean( arr ) );
        df[ i, CL_MEDIAN     ] = sfloat( median( arr ) );
        df[ i, CL_STDDEV     ] = sfloat( Statistics.std( arr ) );
        df[ i, CL_MIN        ] = sfloat( minimum( arr ) );
        df[ i, CL_MAX        ] = sfloat( maximum( arr ) );
    end

    pref = longest_common_prefix( df[ :, CL_SIMULATION ] );
    suff = longest_common_suffix( df[ :, CL_SIMULATION ] );
    k = length( pref );
    k_suff = length( suff );
    for  i  ∈ 1:nrow( df )
        df[ i, CL_SIMULATION ] = chop(  df[ i, CL_SIMULATION ], head=k, tail=k_suff );
    end


    rm_boring_column( df, CL_RUNS );
    rm_boring_column( df, CL_SUCC_RUNS );
    rm_boring_column( df, CL_FAIL_RUNS );

    write_latex_table( "out/results.tex" , df );
    println( "Created file: ", "out/results.tex" );
end
