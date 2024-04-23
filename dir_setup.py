from subprocess import call

checkpoint = "run/473.astar_biglakes_ref.1844.0.50.gz"

call("mkdir " + checkpoint, shell=True)
call("ln -s ../../build/uarchsim/721sim " + checkpoint, shell=True)
call("ln -s /mnt/designkits/spec_2006_2017/O2_fno_bbreorder/app_storage/pk " + checkpoint, shell=True)
call("ln -s ../../makefile_gen1.py " + checkpoint, shell=True)
call("./makefile_bash.sh " + checkpoint + " makefile_gen1.py", shell=True)

