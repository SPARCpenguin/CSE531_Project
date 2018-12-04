import paramiko
import os
import time
from cProfile import run
import cmd
import socket
import threading
import glob
import ctypes

###################
# Global Settings #
###################
sshKey = paramiko.RSAKey.from_private_key_file(os.path.expanduser("~/simpleFileLockServiceSSH"))
serverPrefix = "server_"
clientPrefix = "client_"
serverPort = "9001"
testPath = "/mnt/nfsShare/test"
logCabinBinaryName = "LogCabin"
ft_serverBinaryName = "FT_SimpleFileLock_Server"
serverBinaryName = "SimpleFileLock_Server"
clientBinaryName = "SimpleFileLock_Client"
clusterServersNoSpace = "server_1:5254,server_2:5254,server_3:5254,server_4:5254,server_5:5254"
clusterServers = "server_1:5254 server_2:5254 server_3:5254 server_4:5254 server_5:5254"

goldenFiles = ["Space: the final frontier. These are the voyages of the starship Enterprise. Its continuing mission: to explore strange new worlds, to seek out new life and new civilizations, to boldly go where no one has gone before","A long time ago in a galaxy far, far away... STAR WARS It is a period of civil war. Rebel spaceships, striking from a hidden base, have won their first victory against the evil Galactic Empire. During the battle, Rebel spies managed to steal secret plans to the Empire's ultimate weapon, the DEATH STAR, an armored space station with enough power to destroy an entire planet. Pursued by the Empire's sinister agents, Princess Leia races home aboard her starship, custodian of the stolen plans that can save her people and restore freedom to the galaxy"]

class clientThread (threading.Thread):
    def __init__(self, threadID, clientName, sshSession, scriptName, runIndex):
        threading.Thread.__init__(self)
        self.threadID = threadID
        self.clientName = clientName
        self.sshSession = sshSession
        self.scriptName = scriptName
        self.runIndex = runIndex
    def run(self):
        # Open an SSH session and write commands to stdin
        stdin, stdout, stderr = self.sshSession.exec_command("bash")
        stdin.write("cd " + testPath + "\n")
        cmd = "../simpleFileLockService/bin/" + clientBinaryName + " " + socket.gethostbyname("server_5") + " " + self.clientName + " 1 " + serverPort + " " + self.scriptName + " > log/" + str(self.runIndex) + "_" + self.clientName + "_cmd.log 2>&1\n"
        print cmd
        stdin.write(cmd)
        stdin.write("exit\n")
        stdout.channel.exit_status_ready()
        time.sleep(1)
        print "Client " + str(self.threadID) + " finished\n"
        
def runTest(numClients, numServers, scripts, numFailures, runIndex):
    """
    Run a single test of the Fault Tolerant SimpleFileLockService
    This takes numClients, numServers, and the test scripts to run.
    It SSHs into the requested number of servers and starts up the
    Fault Tolerant SimpleFileLockService on each server.  It then
    starts up the requested number of clients.
    """
    clientSSH = [None]*numClients
    thread = [None]*numClients
    serverSSH = [None]*numServers
    
    # Loop through clients and kill any previously running processes
    for i in range(0,numClients):
        # Open Client SSH sessions
        host = clientPrefix + str(i + 1)
        clientSSH[i] = paramiko.SSHClient()
        clientSSH[i].set_missing_host_key_policy(paramiko.AutoAddPolicy())
        clientSSH[i].connect(hostname=host, username="client", pkey=sshKey)
        
        stdin, stdout, stderr = clientSSH[i].exec_command("killall -9 " + clientBinaryName)
        stdout.channel.exit_status_ready()

    # Loop through servers and kill any previously running processes
    for i in range(0,numServers):
        # Open Server SSH sessions
        host = serverPrefix + str(i + 1)
        serverSSH[i] = paramiko.SSHClient()
        serverSSH[i].set_missing_host_key_policy(paramiko.AutoAddPolicy())
        serverSSH[i].connect(hostname=host, username="server", pkey=sshKey)
        
        # Need to kill log cabin, remove storage directory, and kill FT SFL service
        if numServers > 1:
            # The log cabin service is running on every server, so kill it
            stdin, stdout, stderr = serverSSH[i].exec_command("bash")
            stdin.write("killall -9 " + logCabinBinaryName + "\n")

            if i == (numServers - 1):
                # We only need to clean up the storage directory once
                # and since the filesystem is shared, we'll just remove the
                # directory after connecting to the last server
                stdin.write("rm -rf " + testPath + "/storage\n")
                # The simple file locking service only needs to run on one
                # machine, so the last server will be used for this as wel.
                # kill any running copies
                stdin.write("killall -9 " + ft_serverBinaryName + "\n")

            stdin.write("exit\n")
            stdout.channel.exit_status_ready()
        # Just need to kill SFL service
        else:
            stdin, stdout, stderr = serverSSH[i].exec_command("bash")
            stdin.write("killall -9 " + serverBinaryName + "\n")
            stdin.write("rm -rf " + testPath + "/*.txt\n")
            stdin.write("rm -rf " + testPath + "/incarnation*\n")
            stdin.write("exit\n")
            stdout.channel.exit_status_ready()

    time.sleep(1)

    # Loop through servers and start up logcabin cluster
    for i in range(0,numServers):
        host = serverPrefix + str(i + 1)
        # Need to startup log cabin as well as FT SFL service
        if numServers > 1:
            # Bootstrap logCabin to designate an inital leader.
            # We'll use the first server for this
            if i == 0:
                stdin, stdout, stderr = serverSSH[i].exec_command("bash")
                cmd = "cd " + testPath + "\n"
                print cmd
                stdin.write("cd " + testPath + "\n")
                cmd = "../logcabin/build/" + logCabinBinaryName + " --config logCabin-" + host + ".conf --bootstrap > log/" + str(runIndex) + "_" + host + "_bootstrap.log 2>&1\n"
                print cmd
                stdin.write(cmd)
                stdin.write("exit\n")
                stdout.channel.exit_status_ready()
                time.sleep(1)
                
            # Bring up logCabin server on each server
            stdin, stdout, stderr = serverSSH[i].exec_command("bash")
            stdin.write("cd " + testPath + "\n")
            cmd = "nohup ../logcabin/build/" + logCabinBinaryName + " --config logCabin-" + host + ".conf > log/" + str(runIndex) + "_" + host + "_logcabin.log 2>&1 &\n"
            print cmd
            stdin.write(cmd)
            stdin.write("exit\n")
            stdout.channel.exit_status_ready()
            time.sleep(1)
     
            if i == (numServers - 1):
                # Reconfigure the cluster to include all logCabin servers
                stdin, stdout, stderr = clientSSH[0].exec_command("bash")
                stdin.write("cd " + testPath + "\n")
                cmd = "../logcabin/build/Examples/Reconfigure --cluster=" + clusterServersNoSpace + " set " + clusterServers + " > log/" + str(runIndex) + "_client_1_reconfigure.log 2>&1\n"
                print cmd
                stdin.write(cmd)
                stdin.write("exit\n")
                stdout.channel.exit_status_ready()
                time.sleep(1)

                # Start up FT Simple File Locking Service
                stdin, stdout, stderr = serverSSH[i].exec_command("bash")
                stdin.write("cd " + testPath + "\n")
                stdin.write("nohup ../simpleFileLockService/bin/" + ft_serverBinaryName + " > log/" + str(runIndex) + "_" + host + "_simplefilelockservice.log 2>&1 &\n")
                stdin.write("exit\n")
                stdout.channel.exit_status_ready()
                time.sleep(1)
        # Just need to startup SFL service
        else:
            # Start up Simple File Locking Service
            stdin, stdout, stderr = serverSSH[i].exec_command("bash")
            stdin.write("cd " + testPath + "\n")
            stdin.write("nohup ../simpleFileLockService/bin/" + serverBinaryName + " 9001 > log/" + str(runIndex) + "_" + host + "_simplefilelockservice.log 2>&1 &\n")
            stdin.write("exit\n")
            stdout.channel.exit_status_ready()
            time.sleep(.5)
 
    # Loop through the client list and kick off the client threads
    for i in range(0,numClients):
        host = clientPrefix + str(i + 1)
        thread[i] = clientThread(i, host, clientSSH[i], scripts[i], runIndex)
        thread[i].start()
        print "Client " + str(i) + " started"
         
    # If a failure is requested, trigger the failure(s)
    if numFailures > 0:
        print("Failure requested")
         
        # Delay a little to ensure the threads spawned and started executing
        time.sleep(.2)
         
        # Kill requested number of servers
        for i in range(0,numFailures):      
            if numServers > 1:
                stdin, stdout, stderr = serverSSH[i].exec_command("killall -9 " + logCabinBinaryName)
                stdout.channel.exit_status_ready()
            else:
                stdin, stdout, stderr = serverSSH[i].exec_command("killall -9 " + serverBinaryName)
                stdout.channel.exit_status_ready()
 
        # Need to kill clients as well so the script won't hang on the "join" call
        for i in range(0,numClients):      
                stdin, stdout, stderr = clientSSH[i].exec_command("killall -9 " + clientBinaryName)
                stdout.channel.exit_status_ready()
 
    print "waiting to join"
    # Wait for client threads to exit
    for i in range(0,numClients):
        thread[i].join()
         
    print "done!"
     
    testPassed = True
     
    if numServers > 1:
        for i in range(0,numClients):
            outfile = "client_" + str(i+1) + ":BestSpaceOpera.txt"
            stdin, stdout, stderr = clientSSH[i].exec_command("bash")
            stdin.write("cd " + testPath + "\n")
            cmd = "../logcabin/build/Examples/TreeOps --cluster=" + clusterServersNoSpace + " read " + outfile + " --verbosity=SILENT\n"
            print cmd
            stdin.write(cmd)
            stdin.write("exit\n")
            stdout.channel.exit_status_ready()
            time.sleep(1)
            
            output = stdout.read()
            
            print output
            print goldenFiles[i]
            
            if output != goldenFiles[i]:
                testPassed = False
    else:
        time.sleep(2)
     
        for i in range(0,numClients):
            outFile = "/nfsShare/test/client_" + str(i+1) + ':BestSpaceOpera.txt'
            f = open(outFile, 'r')
            output = f.read()

            if output != goldenFiles[i]:
                testPassed = False

    # Close SSH sessions
    for i in range(0,numClients):
        clientSSH[i].close()
        
    for i in range(0,numServers):
        serverSSH[i].close()
    
    return testPassed

scripts = ["StarTrek.cmd", "StarWars.cmd"]

logFile = "/nfsShare/test/testStatus.out"

f = open(logFile, 'w')

for i in range(0,100l):
    status = runTest(2,5, scripts, 0, i)
    value = ""
    
    if status == True:
        value = "PASSED\n"
    else:
        value = "FAILED\n"
    
    f.write("Run " + str(i) + ": " + value)
    f.flush()
    
f.close()