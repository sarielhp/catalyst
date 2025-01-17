#! /usr/bin/julia

using CSV
using DataFrames
using Format
using Statistics
using Plots
using Random;

function  read_distribution( filename )
    arr = open( filename ) do f
        readlines(f) |> (s->parse.(Float64, s))
    end
    return  arr;
end

function  plot_it( sequences, labels_list, out_fn  )
    q = plot(sequences, labels=labels_list, yaxis = ("seconds"),
    xaxis=("Runs sorted by running times") );
    savefig( q, out_fn );
    println( "   Created: ", out_fn );
end

function  simulations_1_8()
    ODIR="out/";

    counter_search = read_distribution( "results/1_8/rt_counter_search.txt" );
    wide_search = read_distribution( "results/1_8/rt_wide_search.txt" );
    parallel_search = read_distribution( "results/1_8/rt_parallel_search.txt" );

    plot_it( [sort(counter_search), sort( wide_search ), sort( parallel_search ) ],
             ["Counter search" "Wide search" "Parallel search" ],
             ODIR*"simulations.pdf" );

    q = histogram( [ sort(counter_search) sort(wide_search) sort(parallel_search)],
                    labels=["Counter search" "wide search" "parallel search"],
                    yaxis = ("Runs"),
                    xaxis=("Running times in seconds") );
    savefig( q, "out/1_8_results.pdf" );
end

function  (@main)( ARGS )
    simulations_1_8();

    return  0;
end 
