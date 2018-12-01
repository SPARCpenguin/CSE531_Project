import paramiko
import os
from cProfile import run

###################
# Global Settings #
###################
sshKey = paramiko.RSAKey.from_private_key_file(os.path.expanduser("~/simpleFileLockServiceSSH"))
serverPrefix = "server_"
clientPrefix = "client_"
serverPort = "1234"
sourceBinaryPath = os.path.expanduser("~/git/CSE531_Project/simpleFileLockService/bin/")
serverBinaryPath = "/home/server/bin/"
clientBinaryPath = "/home/client/bin/"
serverBinaryName = "FT_SimpleFileLock_Server"
clientBinaryName = "FT_SimpleFileLock_Client"

def runTest(numClients, numServers, script):
    """
    Run a single test of the Fault Tolerant SimpleFileLockService
    This takes numClients, numServers, and the test script to run
    SSHs into the requested number of servers and starts up the
    Fault Tolerant SimpleFileLockService on each server.  It then
    starts up the requested number of clients.
    """
    
    # Generate the server startup string
    serverKillString = "killall -9 " + serverBinaryName
    serverExecString = "nohup " + serverBinaryName + " " + serverPort + "&"
    
    # Copy client and server executables
    for i in range(0,numServers):
        host = serverPrefix + str(i + 1)
        
        ssh = paramiko.SSHClient()
        ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        ssh.connect(hostname=host, username="server", pkey=sshKey)
        ssh.exec_command(serverKillString)
        ssh.exec_command("mkdir " + serverBinaryPath)
        sftp = ssh.open_sftp()
        sftp.put(sourceBinaryPath + serverBinaryName, serverBinaryPath + serverBinaryName)
        sftp.close()
        ssh.exec_command("chmod +x " + serverBinaryPath + serverBinaryName)
        #ssh.exec_command("source ~/.bashrc")
        #ssh.exec_command(serverExecString)
        ssh.close()

runTest(1, 1, "./scripts/StarTrek.cmd")