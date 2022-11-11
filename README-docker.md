# DTNME Container

Contains DTNME and associated tooling. This is all contained within a container, using multi-stage builds to:
- Build DTNME
- Prepare container

Once built, the container can be instantiated via docker/kubernetes. The launcher.sh script will start, which will:
- Verify if a bundle cache is located at /var/dtn, and a bundle database is found at /var/dtn/db. If not, run dtnme --init-db to instantiate the database. The /var/dtn directory is typically provided by an external mountpoint (for docker) or a persistent volume claim (for kubernetes)
- Start DTNME. This becomes the main process of the container, so if dtnme crashes the container will terminate/restart, depending on deployment.

Note: DTNME will start a console on TCP port 5050. If this port is externally mapped, the CLI can be reached via telnet. 
# Building
From the root directory of the project, run:
```
docker build -t <tag> .
```
Where tag is a tag in the docker-standard name:tag format, eg nasa/dtnme:latest. 

# Running
The container requires access to a DTNME configuration file, which must be mounted into the container. By default, the container will look for a configuration file in /conf/dtnme.conf.
If another location is desired, the full path may be passed to the container within docker run. For example, if the user wants to use the "standard" dtn.conf file from within the container, run:
```
docker run -p 4556:14556 -it nasa/dtnme /etc/dtn.conf
```
If you want to use a container from the host system, it must be mounted into the VM:
```
docker run -v <absolute path to config directory>/:/conf/ -p 4556:14556 -it nasa/dtnme /conf/<desired config file>.conf
```

Finally, if bundle persistance (over multiple executions) is desired, the /var/dtn directory must be stored in a PersistantVolume (for Kubernetes) or a mount (for Docker):
```
docker run -v <absolute path to bundle directory>/:/var/dtn/ -p 4556:14556 -it nasa/dtnme /conf/<desired config file>.conf
```

If the user wishes to daemonize the container: replace -it with -d

