from subprocess import call

checkpoint = "473.astar_biglakes_ref.1844.0.50.gz"
checkpoint_2 = "473.astar_biglakes_ref"

command = "atool-simenv mkgen " + checkpoint_2 + " --checkpoint " + checkpoint
call(command, shell=True)

