# What is catalyst?

Catalyst is a user space scheduler/simulator intended to run Las Vegas
algorithms so that they get a successful run faster.
A theoretical paper describing the algorithms implemented is
[Quickly Avoiding a Random
Catastrophe](https://arxiv.org/abs/2503.04633). An applied paper is in
the works, but generally speaking a speedup up by a factor of 3 seems
to be quite common for cases where catalyst helps.

# Compilation

The program is written using C++ and unix process signals
(kill/stop/etc). It was tested on linux extensively, but it should
theoretically work correctly on any POSIX complaint system. To
compile, simply use *make*.


# How does it work?




1. To compile use make.

# Usage

2. Create a program to be used by catalyst:

   Create a script/program (say named) ALG. The program would be
   initialized as follows:

   ALG  work_dir/ [success_file_name]

   The [success_file_name] should be created by the end of ALG
   execution only if it was successful - it is a good idea if the file
   would contain the command line the program was called on, so that
   one can identify which copy of the program/directory contains the
   computed data of the successful fun.

3. Its a good idea to create script that starts catalyst. An example
   is provided in the script "scripts/run_it". It call catalyst, as
   follows;

   ./catalyst ALG temp_work_dir/

   Catalyst runs many copies of ALG - the 1th created copy is run at
   directory temp_word_dir/000001/, etc.

WARNNING: Catalyst deletes the temp_work_dir/ and recreates it. Do not
store any information you want to keep in this directory.


Modes
=====

- Wide search:

  ./catalist -a ALG global_work_dir/

  Pause/resume the proceses implemnting the wide search described in
  the writeup.


- Parallel search

  ./catalist -p ALG global_work_dir/

  Runs as many parallel copies as there are threads in the
  system. Stops as soon as one of them succeeds.


- Counter mode

  ./catalist ALG global_work_dir/

  Implements the conter mode in the stop/restart model. Using
  naturally as many threads as there are threads in the system.




Limitations
===========

Catalyst should compile on any posix compliant system - currently
tested only on linux.

================================================================
Compilation
-----------

You might need to install procps:

    apt-get install libproc2-dev
