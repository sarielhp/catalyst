# What is catalyst?

Catalyst is a user space scheduler/simulator intended to run Las Vegas
algorithms so that they get a successful run faster.
A theoretical paper describing the algorithms implemented is
[Quickly Avoiding a Random
Catastrophe](https://arxiv.org/abs/2503.04633). An applied paper is in
the works, but  a speedup  by a factor of 3 seems
to be quite common for cases where catalyst helps.

## Basic idea/usage 
The basic idea is that the user provides a program (say called) ALG
that runs and try to solve some search problem - importantly ALG has
to be randomized. 

### For catalyst to be effective
For catalyst to be effective, ALG need to have the
behavior that with (usually) small probability it finds a solution
quickly, and otherwise it might take much longer to find a
solution. Random motion planers, for certain inputs

### What catalyst do

Catalyst would run "many" copies of ALG in parallel, killing some of
the copies if they exceeds certain prespecified TTL (time to live). As
soon as one of the runs of ALG succeeds, catalyst would kill all the
running processes, and terminate. See the 
[paper](https://arxiv.org/abs/2503.04633) for more details.




# To use 

1. Create a program to be used by catalyst:

   Create a script/program (say named) ALG. The program be run from
   the command line with the following interface:

   >   ./ALG  work_dir/ [success_file_name]

   The [success_file_name] should be created by the end of ALG
   execution only if the run was successful - it is a good idea if
   this text file contains the command line the program was
   called on, so that one can identify which copy of the
   program/directory contains the computed data of the successful fun.

2. Its a good idea to create script that starts catalyst. An example
   is provided in the script "scripts/run_it". It call catalyst, as
   follows;

   > ./catalyst ALG temp_work_dir/

   Catalyst runs many copies of ALG - the 1th created copy is run at
   directory temp_word_dir/000001/, etc.

WARNING: Catalyst deletes the temp_work_dir/ and recreates it. Do not
store any information you want to keep in this directory. Also, copy
all the files in successful run work directory after the run
terminates if you want to use the computed results.


# Modes

Catalyst implements various simulation strategies. Do
   
   > ./catalyst  --help
   
To get command line options. Generally speaking, we have the following
modes:

- Counter search

  > ./catalyst ALG global_work_dir/

  Implements the counter mode in the stop/restart model. Using
  naturally as many threads as there are threads in the system.

- Caching processes 

  > ./catalyst -m ALG global_work_dir/

  Suspends/resumes some processes instead of killing them. Performs
  betters in many cases. The above example run counter search with
  this processes cache.
  
- Random search

    + "counter" distribution:

    > ./catalyst -r ALG global_work_dir/

    + $\zeta_2$ distribution:

    > ./catalyst -R ALG global_work_dir/
  
  


- Wide search:

  ./catalyst -a ALG global_work_dir/

  Pause/resume the processes implementing the wide search described in
  the paper.

- Parallel search

  ./catalyst -p ALG global_work_dir/

  Runs as many parallel copies as there are threads in the
  system. Stops as soon as one of them succeeds.


### Limitations

Catalyst should compile on any posix compliant system - currently
tested only on linux.


### Compilation

The program is written using C++ and unix process signals
(kill/stop/etc). It was tested on Linux extensively, but it should
theoretically work correctly on any POSIX complaint system. To
compile, simply use *make*. The program uses high level process
information provided by Linux, and it currently does NOT use libproc2.
