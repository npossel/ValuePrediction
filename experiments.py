from subprocess import call

call("source /share/ece721s24/benchmarks/app_storage/activate.source", shell=True)

vpq_size = [150, 175, 200, 225]
index_bits = [6, 7, 8, 9, 10]
tag_bits = [0, 5, 10]
confmax = [3, 7, 15]
confinc = [1, 2, 3]
confdec = [0, 1, 2, 3]
replace_stride = [3, 7, 15]
replace = [3, 7, 15]
exec = "721sim"
sim_name = " --sim-name "
sim_flags = " --sim-flags \"--vp-enable=1 --vp-svp="
sim_launch = "sim-launch --721sim ./" + exec
harmonic_mean = "harmonic_mean.txt"
file2 = open(harmonic_mean, "w+")


for item in vpq_size:
    name = "size_" + str(item)
    name_file = name + ".txt"
    flags = str(item) + ",0,10,10,15,1,0,15,15,1,0,1,0 --mdp=3,31 --perf=0,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -e10000000\""
    sim_name2 = sim_name + name
    sim_flags2 = sim_flags + flags
    command = sim_launch + sim_name2 + sim_flags2
    call(command, shell=True)
    collect = "sim-collect " + name + " > " + name_file
    call(collect, shell=True)
    file1 = open(name_file, "r+")
    lines = file1.readlines()
    ipc = 0.0
    for i in range(1, len(lines)):
        temp = lines[i].split(",")
        ipc += 1/float(temp[1])
    total = '%.2f' %(15/ipc)
    file1.write("The harmonic mean ipc is " + total + "\n")
    file2.write(name + " harmonic mean ipc is " + total + "\n")
    file1.close()

for item in index_bits:
    name = "index_" + str(item)
    name_file = name + ".txt"
    flags = "250,0," + str(item) + ",10,15,1,0,15,15,1,0,1,0 --mdp=3,31 --perf=0,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -e10000000\""
    sim_name2 = sim_name + name
    sim_flags2 = sim_flags + flags
    command = sim_launch + sim_name2 + sim_flags2
    call(command, shell=True)
    collect = "sim-collect " + name + " > " + name_file
    call(collect, shell=True)
    file1 = open(name_file, "r+")
    lines = file1.readlines()
    ipc = 0.0
    for i in range(1, len(lines)):
        temp = lines[i].split(",")
        ipc += 1/float(temp[1])
    total = '%.2f' %(15/ipc)
    file1.write("The harmonic mean ipc is " + total + "\n")
    file2.write(name + " harmonic mean ipc is " + total + "\n")
    file1.close()

for item in tag_bits:
    name = "tag_" + str(item)
    name_file = name + ".txt"
    flags = "250,0,10," + str(item) + ",15,1,0,15,15,1,0,1,0 --mdp=3,31 --perf=0,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -e10000000\""
    sim_name2 = sim_name + name
    sim_flags2  = sim_flags + flags
    command = sim_launch + sim_name2 + sim_flags2
    call(command, shell=True)
    collect = "sim-collect " + name + " > " + name_file
    call(collect, shell=True)
    file1 = open(name_file, "r+")
    lines = file1.readlines()
    ipc = 0.0
    for i in range(1, len(lines)):
        temp = lines[i].split(",")
        ipc += 1/float(temp[1])
    total = '%.2f' %(15/ipc)
    file1.write("The harmonic mean ipc is " + total + "\n")
    file2.write(name + " harmonic mean ipc is " + total + "\n")
    file1.close()
    
for item in confmax:
    name = "max_" + str(item)
    name_file = name + ".txt"
    flags = "250,0,10,10," + str(item) + ",1,0,15,15,1,0,1,0 --mdp=3,31 --perf=0,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -e10000000\""
    sim_name2 = sim_name + name
    sim_flags2  = sim_flags + flags
    command = sim_launch + sim_name2 + sim_flags2
    call(command, shell=True)
    collect = "sim-collect " + name + " > " + name_file
    call(collect, shell=True)
    file1 = open(name_file, "r+")
    lines = file1.readlines()
    ipc = 0.0
    for i in range(1, len(lines)):
        temp = lines[i].split(",")
        ipc += 1/float(temp[1])
    total = '%.2f' %(15/ipc)
    file1.write("The harmonic mean ipc is " + total + "\n")
    file2.write(name + " harmonic mean ipc is " + total + "\n")
    file1.close()
    
for item in confinc:
    name = "inc_" + str(item)
    name_file = name + ".txt"
    flags = "250,0,10,10,15," + str(item) + ",0,15,15,1,0,1,0 --mdp=3,31 --perf=0,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -e10000000\""
    sim_name2 = sim_name + name
    sim_flags2  = sim_flags + flags
    command = sim_launch + sim_name2 + sim_flags2
    call(command, shell=True)
    collect = "sim-collect " + name + " > " + name_file
    call(collect, shell=True)
    file1 = open(name_file, "r+")
    lines = file1.readlines()
    ipc = 0.0
    for i in range(1, len(lines)):
        temp = lines[i].split(",")
        ipc += 1/float(temp[1])
    total = '%.2f' %(15/ipc)
    file1.write("The harmonic mean ipc is " + total + "\n")
    file2.write(name + " harmonic mean ipc is " + total + "\n")
    file1.close()
    
for item in confdec:
    name = "dec_" + str(item)
    name_file = name + ".txt"
    flags = "250,0,10,10,15,1," + str(item) + ",15,15,1,0,1,0 --mdp=3,31 --perf=0,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -e10000000\""
    sim_name2 = sim_name + name
    sim_flags2  = sim_flags + flags
    command = sim_launch + sim_name2 + sim_flags2
    call(command, shell=True)
    collect = "sim-collect " + name + " > " + name_file
    call(collect, shell=True)
    file1 = open(name_file, "r+")
    lines = file1.readlines()
    ipc = 0.0
    for i in range(1, len(lines)):
        temp = lines[i].split(",")
        ipc += 1/float(temp[1])
    total = '%.2f' %(15/ipc)
    file1.write("The harmonic mean ipc is " + total + "\n")
    file2.write(name + " harmonic mean ipc is " + total + "\n")
    file1.close()
    
for item in replace_stride:
    name = "stride_" + str(item)
    name_file = name + ".txt"
    flags = "250,0,10,10,15,1,0," + str(item) + ",15,1,0,1,0 --mdp=3,31 --perf=0,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -e10000000\""
    sim_name2 = sim_name + name
    sim_flags2  = sim_flags + flags
    command = sim_launch + sim_name2 + sim_flags2
    call(command, shell=True)
    collect = "sim-collect " + name + " > " + name_file
    call(collect, shell=True)
    file1 = open(name_file, "r+")
    lines = file1.readlines()
    ipc = 0.0
    for i in range(1, len(lines)):
        temp = lines[i].split(",")
        ipc += 1/float(temp[1])
    total = '%.2f' %(15/ipc)
    file1.write("The harmonic mean ipc is " + total + "\n")
    file2.write(name + " harmonic mean ipc is " + total + "\n")
    file1.close()
    
for item in replace:
    name = "replace_" + str(item)
    name_file = name + ".txt"
    flags = "250,0,10,10,15,1,0,15," + str(item) + ",1,0,1,0 --mdp=3,31 --perf=0,0,0,1 -t --cbpALG=0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=8 --dw=8 --iw=16 --rw=8 -e10000000\""
    sim_name2 = sim_name + name
    sim_flags2  = sim_flags + flags
    command = sim_launch + sim_name2 + sim_flags2
    call(command, shell=True)
    collect = "sim-collect " + name + " > " + name_file
    call(collect, shell=True)
    file1 = open(name_file, "r+")
    lines = file1.readlines()
    ipc = 0.0
    for i in range(1, len(lines)):
        temp = lines[i].split(",")
        ipc += 1/float(temp[1])
    total = '%.2f' %(15/ipc)
    file1.write("The harmonic mean ipc is " + total + "\n")
    file2.write(name + " harmonic mean ipc is " + total + "\n")
    file1.close()

file2.close()
