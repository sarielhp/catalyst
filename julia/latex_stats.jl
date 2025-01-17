#! /bin/julia

using TimerOutputs
using Printf
using CSV, DataFrames
using PrettyTables


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
    s = @sprintf( "%.3f", num );
    return  s;
end

function  (@main)(ARGS)

    if  length( ARGS ) == 0
        println( "latex_table [run_1.log] ... [run_t.log]\n" );
        exit( -1 );
    end

    df = DataFrame();

    CL_SIMULATION = "Simulation";
    CL_RUNS = "Runs";
    CL_MEAN = "Mean";
    CL_MEDIAN = "Median";
    CL_STDDEV = "Std Dev";
    CL_MIN = "Min";
    CL_MAX = "Max";

    add_col( df, CL_SIMULATION, CL_RUNS, CL_MEAN, CL_MEDIAN, CL_STDDEV,
        CL_MIN, CL_MAX );

    for  s ∈ ARGS
        arr = open( s ) do f
            readlines(f) |> (s->parse.(Float64, s))
        end
        df_add_row( df );

        i = nrow( df );
        df[ i, CL_SIMULATION ] = s;
        df[ i, CL_RUNS   ] = string( length( arr ) );
        df[ i, CL_MEAN   ] = sfloat( mean( arr ) );
        df[ i, CL_MEDIAN ] = sfloat( median( arr ) );
        df[ i, CL_STDDEV ] = sfloat( Statistics.std( arr ) );
        df[ i, CL_MIN    ] = sfloat( minimum( arr ) );
        df[ i, CL_MAX    ] = sfloat( maximum( arr ) );
    end

    write_latex_table( "out/results.tex" , df );

end
