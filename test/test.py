import paramiko
import os
import time
from cProfile import run
import cmd

###################
# Global Settings #
###################
sshKey = paramiko.RSAKey.from_private_key_file(os.path.expanduser("~/simpleFileLockServiceSSH"))
serverPrefix = "server_"
clientPrefix = "client_"
serverPort = "9001"
homePath = "/mnt/nfsShare"
logCabinBinaryName = "LogCabin"
ft_serverBinaryName = "FT_SimpleFileLock_Server"
serverBinaryName = "SimpleFileLock_Server"
clientBinaryName = "SimpleFileLock_Client"
clusterServersNoSpace = "server_1:5254,server_2:5254,server_3:5254,server_4:5254,server_5:5254"
clusterServers = "server_1:5254 server_2:5254 server_3:5254 server_4:5254 server_5:5254"
scriptName = "StarWars.cmd"

def runTest(numClients, numServers, script):
    """
    Run a single test of the Fault Tolerant SimpleFileLockService
    This takes numClients, numServers, and the test script to run
    SSHs into the requested number of servers and starts up the
    Fault Tolerant SimpleFileLockService on each server.  It then
    starts up the requested number of clients.
    """
    
    # Kill any SFL service binaries that are running
    cmd = "killall -9 " + serverBinaryName
    print cmd
    os.system(cmd)
    cmd = "killall -9 " + ft_serverBinaryName
    print cmd
    os.system(cmd)
    
    # Loop through servers and kill any previously running processes
    for i in range(0,numServers):
        host = serverPrefix + str(i + 1)
        ssh = paramiko.SSHClient()
        ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        ssh.connect(hostname=host, username="server", pkey=sshKey)
        ssh.exec_command("killall -9 " + logCabinBinaryName)
        
        # We only need to clean up the storage directory once, so do it on the last round
        if i == (numServers - 1):
            ssh.exec_command("rm -rf " + homePath + "/test/storage")
        
        ssh.close()

    # Loop through servers and start up logcabin cluster
    for i in range(0,numServers):
        host = serverPrefix + str(i + 1)
        ssh = paramiko.SSHClient()
        ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        ssh.connect(hostname=host, username="server", pkey=sshKey)

        # Bootstrap logCabin to designate an inital leader
        if i == 0:
            ssh.exec_command("screen -m -d bash -c \"cd " + homePath + "/test && ../logcabin/build/" + logCabinBinaryName + " --config logCabin-" + host + ".conf --bootstrap\"")
            time.sleep(1)
        
        # Bring up logCabin server
        ssh.exec_command("screen -m -d bash -c \"cd " + homePath + "/test && ../logcabin/build/" + logCabinBinaryName + " --config logCabin-" + host + ".conf\"")
        
        ssh.close()
        
    # Reconfigure the cluster to include all logCabin servers
    os.system("bash -c \"cd /nfsShare/ && logcabin/build/Examples/Reconfigure --cluster=" + clusterServersNoSpace + " set " + clusterServers + "\"")

    # Start up SFL service
    service = ""
    if numServers > 1:
        service = ft_serverBinaryName
    else:
        service = serverBinaryName
        
    os.system("screen -m -d bash -c \"cd /nfsShare/ && simpleFileLockService/bin/" + service + "\"")

    time.sleep(5)

    os.system("bash -c \"cd /nfsShare/ && simpleFileLockService/bin/" + clientBinaryName + " 127.0.0.1 test 1 " + serverPort + " test/" + scriptName + "\"")

runTest(1, 5, "./scripts/StarTrek.cmd")