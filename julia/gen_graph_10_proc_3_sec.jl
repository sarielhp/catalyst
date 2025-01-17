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

### Generate graphs for the case where the algorithm being simulated
### runs roughly for three seconds. Then it randomly decides if to
### stop with probability 10%, otherwise it continues forever.
function  gen_10_prec_3_sec()
    ODIR="out/";

    BDIR="results/10_proc_stop_3_sec/";
    counter_search = read_distribution( BDIR * "rt_counter_search.txt" );
    wide_search = read_distribution( BDIR * "rt_wide_search.txt" );

    plot_it( [sort(counter_search), sort( wide_search ) ],
             ["Counter search" "Wide search"  ],
             ODIR*"10_proc_3_sec_stop.pdf" );

    q = histogram( [ sort(counter_search) sort(wide_search) ],
                    labels=["Counter search" "Wide search"],
                    yaxis = ("Runs"),
                    xaxis=("Running times in seconds") );
    savefig( q, "out/10_proc_3_sec_stop_hist.pdf" );
end

function  (@main)( ARGS )
    gen_10_prec_3_sec();

    return  0;
end 
