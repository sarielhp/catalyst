#! /usr/bin/julia

using CSV
using DataFrames
using Format
using Statistics
using Plots
using Random;

"""
    prob_binom

    computes binomial( n, m ) * p^m (1-p)^{n-m}
"""
function  prob_binom( n::Int64, m::Int64, p::Float64 )
    out::Float64 = 1.0;
    if  ( ( m < 0 )  ||  ( m > n ) )
        return  0.0;
    end

    q = 1.0 - p;
    p_count = m;
    q_count = n-m;
    # n! / (n-m)! m!
    for  i  ∈  1:m
        enm = n - m + i;
        # ( enm / i ) * out
        out = out * ( enm / i );
        while  out >= 1.0
            if  ( p_count > 0 )
                out = out * p;
                p_count = p_count - 1;
                continue;
            end
            if  ( q_count > 0 )
                out = out * q;
                q_count = q_count - 1;
                continue;
            end
            break;
        end
    end
    while   ( p_count > 0 )
        out = out * p;
        p_count = p_count - 1;
    end
    while   ( q_count > 0 )
        out = out * q;
        q_count = q_count - 1;
    end

    return out;
end


##############################################################################
# the probabiliyt of a walk starting at 0 to arrive to m, for the
# first time at time n.
#     F(n,m)=
#     \frac{1}{2^n}
#     \frac{m}{\tfrac{n+m}{2}} \binom{n-1}{\tfrac{n+m-2}{2}}
function   prob_walk( n, m )
    if  ( n < m )
        return  0.0;
    end
    if  ( ( n % 2 ) != ( m % 2 ) )
        return  0.0;
    end
    return  m * prob_binom( n-1, (n+m-2) ÷ 2, 0.5 ) / (n + m)
end

function  load_rt_info( filename )
    df = DataFrame(CSV.File(filename))
    v = [d for d ∈ df[:,1]]

    v = v .+ 1.0;

    return  v;
end


function  load_RRT( filename )
    df = DataFrame(CSV.File(filename))
    v = df."Time"; # Get the column as vector
    v = v .+ 1.0;
    return  v;
end


function  print_sorted( cw )
    cw_sort=sort(cw)
    for x ∈ cw_sort  println( x )  end;
end


function  run_single_threshold( trsh, running_times )
    total = 0.0;
    while true
        p = rand( 1:length( running_times ) );
        if  running_times[ p ] <= trsh
            total = total + running_times[ p ];
            break;
        end
        total = total + trsh;
    end

    return  total;
end


function  run_simulation_exp_single( start, rate, running_times )
    threshold = start;
    total = 0.0;
    while true
        p = rand( 1:length( running_times ) );
        if  running_times[ p ] < threshold
            total = total + running_times[ p ];
            break;
        end
        total = total + threshold;
        threshold = threshold * rate;
    end

    return  total;
end

function  run_simulation_exp( start, rate, running_times )
    res = [ run_simulation_exp_single( start, rate, running_times )
          for i ∈ 1:length( running_times ) ];
    return  res;
end

    #=
    p=histogram( [rrt], bins=30 )
    histogram!( p, [cw], bins=60 )

    savefig( p, "bar_Graph.pdf" );
    =#

function  plot_it( sequences, labels_list, out_fn  )
    q = plot(sequences, labels=labels_list, yaxis = ("seconds"),
    xaxis=("Runs sorted by running times") );
    savefig( q, out_fn );
    println( "   Created: ", out_fn );
end

function  f_i_sq_log( i )
    if i == 1 return 1.0 end;
    if i == 2 return 2.0 end;

    return ( i * log( i )^2 );
end

function  print_function_vals( f, range )
    for r ∈ range
        println( r, " => ", f( r ) );
    end
end


function  run_simulations( cw )

    sim_2 = run_simulation_exp( 1.0, 2.0, cw );
    sort!( sim_2 );
    # println( sim_2 );

    sim_1_1 = run_simulation_exp( 1.0, 1.1, cw );
    sort!( sim_1_1 );

    sim_1_5 = run_simulation_exp( 1.0, 1.1, cw );
    sort!( sim_1_5 );

    sim_1_01 = run_simulation_exp( 1.0, 1.01, cw );
    sort!( sim_1_01 );

    plot_it( [sort(cw), sim_2, sim_1_1, sim_1_01],
        ["CW" "sim 2" "sim 1.1" "sim 1.01" ], ODIR*"sim_2.pdf" );

    println( "Total running time cw       : ", sum( cw ) );
    println( "Total running time sim 2    : ", sum( sim_2 ) );
    println( "Total running time sim 1.5  : ", sum( sim_1_5 ) );
    println( "Total running time sim 1.1  : ", sum( sim_1_1 ) );
    println( "Total running time sim 1.01 : ", sum( sim_1_01 ) );

    for r ∈ 1:100
        rate = 1.0 + r/100.0;
        sim = run_simulation_exp( 1.0, rate, cw );
        println( "Running time ",rate, "       : ", sum( sim ) );
    end
end


function  simulate_sail_one_run( cw, f )
    srt = sample_running_times( cw, length( cw ) )
    srtx = multiply_function( srt, f );
    time = srtx[ argmin( srtx ) ];
    total_time = sum( i -> time/f(i), 1:length( cw ) );

    return  total_time;
end

function  simulate_sail_multiple_runs( cw, f, n )
    return  [ simulate_sail_one_run( cw, f )  for i ∈ 1:n ];
end

function  sample_running_times( cw, n )
    return [cw[ rand( 1:length(cw) ) ]  for i ∈ 1:n ];
end

function  multiply_function( arr, f )
    return [ arr[ i ] * f( i )   for i ∈ 1:length( arr ) ];
end


function  compute_opt_threshold( _cw )
    n = length( _cw );
    cw = sort( _cw );
    cum_arr = cumsum( cw );

    P = fill( 1/length( cw ), length( cw ) );
    F = cumsum( P );

    cum_P_arr = cumsum( cw .* P );

    # partial_means[ i ] = mean( arr[1:i] )
    #                      [weighted by the probabilities in P]
    partial_means = cum_P_arr ./ F;

    ptimes = zeros( n );
    for i ∈ 1:n
        ptimes[ i ] = partial_means[ i ] + ((1-F[i])/F[i]) * cw[ i ];
#        println( i, " : ", F[ i ], "    PT: ", ptimes[ i ], "   ",
#            partial_means[ i ], " cw[i]: ", cw[ i ] );
#        println( i, " : ", F[ i ], "    PT: ", ptimes[ i ]);
    end
    pos = argmin( ptimes );

    println( "Predicted     time (per run): ", ptimes[ pos ] );
    println( "   threhsold  time          : ", cw[ pos ] );
    return  cw[ pos ];

    #println( "\n\n\n", arr );
    #println( means );

end

function main_old(ARGS)

    DIR="data/";
    ODIR="out/";

    cw   = load_rt_info( DIR * "cobweb_runtimes.csv" );
    ocw  = load_rt_info( DIR * "old_cobweb_runtimes.csv" );
    rrt  = load_rt_info( DIR * "rrt_runtimes.csv" );
    orrt = load_rt_info( DIR * "old_rrt_runtimes.csv" );



    f_plot_all = false;
    if   f_plot_all
        plot_it( [sort(cw),sort(rrt),sort(ocw),sort(orrt)],
            ["CW" "RRT" "OLD CW" "OLD RRT"],
            ODIR * "running_times.pdf" );
    end

    println( "CW  median: ", median(cw))
    println( "RRT median: ", median(rrt))

    sim_1_1 = run_simulation_exp( 1.0, 1.1, cw );
    sim_2 = run_simulation_exp( 1.0, 2.0, cw );


    sail =  simulate_sail_multiple_runs( cw, i -> i^2, length( cw ) )
    sail_i =  simulate_sail_multiple_runs( cw, i -> i, length( cw ) )
    sail_2_i =  simulate_sail_multiple_runs( cw, i -> min(1.0,2.0*i),
        length( cw ) )
    sail_sq_log =  simulate_sail_multiple_runs( cw, f_i_sq_log, length( cw ) )

    println( "Total running time cw      : ", sum( cw ) );
    println( "Total running time sim_1_1 : ", sum( sim_1_1 ) );
    println( "Total running time sim_2   : ", sum( sim_2 ) );
    println( "total_time (t/i^2)         : ", sum( sail ) );
    println( "total_time (t/i)           : ", sum( sail_i ) );
    println( "total_time (2t/i)          : ", sum( sail_2_i ) );
    println( "total_time (t/i log^2)     : ", sum( sail_sq_log ) );

    trsh = compute_opt_threshold( cw );
    t_times = [ run_single_threshold( trsh, cw ) for i ∈ 1:length( cw ) ];

    println( "Single threshold running times TOTAL  : ", sum( t_times ) );


    return  0;
end


function main_2(ARGS)
    DIR="data/";
    ODIR="out/";

    v = load_RRT( "data/RRT_Vertex_NN.csv" );

#    flat_4_0 = load_RRT( "data/num_attempts_4.0_start_1.0_mult.csv" );
#    grow_1_1 = load_RRT( "data/num_attempts_2.0_start_1.1_mult.csv" );

    #println( v );
    trsh = compute_opt_threshold( v );
    println( "Optimal threhsold : ", trsh );

    sim_2 = run_simulation_exp( 1.0, 2.0, v );
    sort!( sim_2 );

    sim_1_1 = run_simulation_exp( 2.0, 1.1, v );
    sort!( sim_1_1 );

    trsh_times = [ run_single_threshold( trsh, v ) for i ∈ 1:length( v ) ];

    println( "Single threshold running times TOTAL  : ", sum( trsh_times ) );
    println( "Sim Exp (2)                           : ", sum( sim_2 ) );
    println( "Sim Exp (1.1)                         : ", sum( sim_1_1 ) );
#    println( "Exp 1.1                               : ", sum( grow_1_1 ) );
#    println( "Flat (4.0)                            : ", sum( flat_4_0 ) );
    println( "TOTAL                                 : ", sum( v ) );

#    println( trsh_times );
    plot_it( [sort(v), sort(trsh_times), sort( sim_2 ), sort( sim_1_1 ),
              #sort( grow_1_1 ), sort( flat_4_0 )
             ],
        ["RRT" "Trehshold (Sim)" "sim_2" "sim_1.1" ],
         #"Exp 1.1" "Flat 4.0"
        ODIR*"sim_rrt.pdf"  );

end

function  load_cat_avoid( filename )
    dfa = DataFrame(CSV.File(filename))
    df = filter(row -> row[5] isa Number, dfa)

    nsucc = sum(df[:,6]);

    m = zeros( nsucc, 4 )

    CD = 1;
    ITERS = 2;
    TIME = 3;
    ROUNDS = 4;

    row = 1
    ind = 1;
    for  i in 1:nsucc
        erow = row;
        while  ( df[ erow, 5 ] == 0 )
            erow = erow + 1;
        end
        for  r  in  row:erow
            m[ ind, CD ] += df[ r, 1 ];
            m[ ind, ITERS ] += df[ r, 2 ];
            m[ ind, TIME ]  += df[ r, 3 ];
            m[ ind, ROUNDS ] += 1;
        end
        ind = ind + 1;
        row = erow + 2;
    end
    #println( df );
    #println( m );

    v = m[:, TIME]
    #println( "-------------" )
    #println( v )
    #println( typeof( v ) )

    return  v;
end

strF(x)=format( x, commas=true, precision=0);

function  main_3( ARGS )
    println( "hello!" )
    cat_rts = load_cat_avoid( "data/cat_avoid_200_runs_2.0_start_1.1_mult.csv")
    rrt_rts = load_RRT( "data/RRT_Vertex_NN.csv" );

    #    println( trsh_times );
    rrt_label = "RRT Running times     (total: " *
                     strF(sum( rrt_rts ) )  * ")";
    cat_label = "Catastrophe avoid RTs (total: " *
                     strF(sum( cat_rts ) )  * ")";
    plot_it( [sort(rrt_rts), sort(cat_rts) ],
            [rrt_label cat_label],
            "out/rrt_vs_cat.pdf" );


    q = histogram([sort(rrt_rts), sort(cat_rts) ],
            labels=[rrt_label cat_label],
                  yaxis = ("Runs"),
             xaxis=("Running times in seconds") );
    savefig( q, "out/histo.pdf" );
    println( "   Created: ", "out/histo.pdf" );
end

function  main_4( ARGS )
    m = 10;
    limit = 400*m*m;
    sum = 0.0;
    ex = 0.0;
    for  n ∈ 1:limit
        p = prob_walk( n, m );
        sum = sum + p;
        ex = ex + n * p;
        #        println( "(", n, ",", m, ") : ", prob_walk( n, 10) )
    end
    println( "sum : ", sum )
    println( "ex  : ", ex )
end

function  make_into_probs( ws )
    S = sum( ws );
    probs = Vector{Float64}()

    for  w ∈ ws
        push!( probs, w / S );
    end
    return  probs;
end


# Compute prob <= t.
# Here: ∑_i=1^t probs[ i ] 
function  ProbAtMost( probs, t )
    return  sum( probs[ 1 : t ] );
end


function  ExCondAtMost( probs, t )
    T = ProbAtMost( probs, t );

    sum = 0.0;
    for  i ∈ 1:t
        sum += i * (probs[ i ] / T);
    end
    return  sum;
end


function  ex_running_time_at( probs, t )
    ratio =  (1.0 - ProbAtMost( probs, t )) / ProbAtMost( probs, t );
    println( "ratio: ", ratio );
    v = ExCondAtMost( probs, t ) + ratio * t;
    #println( "vvv = ", v );
    return  v;
end


function  best_threshold( probs )    
    min_pos = 1
    while  ( probs[ min_pos ] == 0 )
        min_pos = min_pos + 1;
    end
    
    min_val = ex_running_time_at( probs, min_pos );
    start = min_pos + 1;
    for  i ∈ start:length( probs )
        val = ex_running_time_at( probs, i );
        println( i, " : ", val );
        if  ( val < min_val )
            min_pos = i;
            min_val = val;
        end
    end
    
    return  min_pos, min_val;
end


function  main_2( ARGS )
    n = 100;

    ws = Vector{Float64}()

    # 30: 0.3623
    # 1:  0.36235
    for  i ∈ 1:n
        push!( ws, 1/i^2 );
    end
    #shuffle!( ws );#sort!( ws );
    sort!(ws );
    for i ∈ 1:n
        push!( ws, 0.0 );
    end
    push!( ws, 150.0 );
    reverse!( ws );
    
    
    probs = make_into_probs( ws );
    #println( ws );
    println( probs );

    pos, val =  best_threshold( probs );
    println( "\n\n\n\n\n===============================================" );
    println( "Pos: ", pos );
    println( "Val: ", val );
    println( "n  : ", n );
end


function  read_distribution( filename )
    arr = open( filename ) do f
        readlines(f) |> (s->parse.(Float64, s))
    end
    return  arr;
end

function  simulations()
    ODIR="out/";

    counter_search = read_distribution( "data/counter_search.txt" );
    wide_search = read_distribution( "data/wide_search.txt" );

    plot_it( [sort(counter_search), sort( wide_search ) ],
        ["Counter search" "wide search" ], ODIR*"simulations.pdf" );

    q = histogram( [ sort(counter_search) sort(wide_search)],
                    labels=["Counter search" "wide search"],
                    yaxis = ("Runs"),
                    xaxis=("Running times in seconds") );
    savefig( q, "out/h_search.pdf" );
end

function  (@main)( ARGS )
    simulations();

    return  0;
end 
