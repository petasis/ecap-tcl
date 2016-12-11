# ecap-tcl
An eCAP adapter for Tcl.
[What is eCAP?](http://www.e-cap.org/) - eCAP is a software interface that allows a network application, such as an HTTP proxy or an ICAP server, to outsource content analysis and adaptation to a loadable module.

For each applicable protocol message being processed, an eCAP-enabled host application supplies the message details to the adaptation module and gets back an adapted message, a "not interested" response, or a "block this message now!" instruction. These exchanges often include message bodies.

[What is ecap-tcl?](https://github.com/petasis/ecap-tcl) - It is an eCAP module that can be loaded into an eCAP host, and directs the messages to [Tcl](https://tcl.tk/), allowing to write adaptation modules in the [Tcl language](https://tcl.tk/).

## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes. See deployment for notes on how to deploy the project on a live system.

Please note that this software has been tested only with [Squid](http://www.squid-cache.org/) host, and Tcl 8.6. Success under other host applications and Tcl versions may vary. Also, this is beta quality software.

### Prerequisites

* [Tcl 8.6](http://www.tcl.tk/software/tcltk/8.6.html) - The Tcl language. Tcl 8.6+ may be available as a package in your Linux distribution. ecap-tcl has been tested with ActiveTcl 8.6.4. (Why is Tcl 8.6 required? The C/C++ part of the adapter has no specific requirements on the version. This requirement comes from the [library file](library/ecap-tcl.in), which uses TclOO, the `dict`, `try` and `lassign` commands.)

* [libecap 1.0.0](http://www.e-cap.org/Downloads) - The eCAP library. ecap-tcl has been tested with version 1.0.0 (it may work with either more recent or earlier versions). ecap-tcl has been based on the adapter sample version 1.0.0. Some Linux distributions may have this library as a package:
  * Fedora 24: dnf install libecap-devel

* A standard UNIX build chain, autoconf, make, g++. (ecap-tcl is written in C++).

### Installing

* Download the ecap-tcl sources:
```
git clone https://github.com/petasis/ecap-tcl.git
cd ecap-tcl
```

* If the `configure` script is missing, generate one:
```
autoreconf
```

* Run the `configure` script:
```
bash configure
```

* Run `make install`:
```
make install
```

The ecap-tcl adapter will be installed in `/usr/local/lib/` as `/usr/local/lib/ecap_adapter_tcl<version>`.

* Configure Squid - Edit squid configuration file (`/etc/squid/squid.conf`), and add the following at the end:
```
#
# eCAP
#
loadable_modules /usr/local/lib/ecap_adapter_tcl0.1/libecap_adapter_tcl0.1.so
ecap_enable on
ecap_service ecapModifier respmod_precache \
  uri=ecap://www.tcl.tk/ecap-tcl \
  service_init_script=/usr/local/lib/ecap_adapter_tcl0.1/sample_scripts/service_init.tcl \
  service_start_script=/usr/local/lib/ecap_adapter_tcl0.1/sample_scripts/service_start.tcl \
  service_stop_script=/usr/local/lib/ecap_adapter_tcl0.1/sample_scripts/service_stop.tcl \
  service_retire_script=/usr/local/lib/ecap_adapter_tcl0.1/sample_scripts/service_retire.tcl \
  threads_number=2 \
  service_thread_init_script=/usr/local/lib/ecap_adapter_tcl0.1/sample_scripts/service_thread_init.tcl \
  service_thread_retire_script=/usr/local/lib/ecap_adapter_tcl0.1/sample_scripts/service_thread_retire.tcl
adaptation_access ecapModifier allow all
```

* Test the adapter in Squid - Run as root: (-N runs in no daemon mode)
```
squid -N
```

## Configuring

ecap-tcl understands the following directives (in `squid.conf`):

* `service_init_script`: expects a path to a Tcl script. It is run in order to initialise the "main" interpreter. The main interpreter will be used for evaluating if threads are disabled (`threads_number=0`). In all other cases, all processing will occur in a pool of threads.

* `service_start_script`: expects a path to a Tcl script. It is run when the host application sends the `start` message, to signal to the adapter the start of its lifecycle. The script is executed in the main interpreter.

* `service_stop_script`: expects a path to a Tcl script. It is run when the host application sends the `stop` message, to signal to the adapter the stop of its lifecycle. The script is executed in the main interpreter.

* `service_retire_script`: expects a path to a Tcl script. It is run when the host application sends the `retire` message, to signal to the adapter the stop of its lifecycle. The script is executed in the main interpreter.

* `threads_number`: expects an integer, denoting the number of threads to use. If different than `0`, the adapter will create and use a thread pool for all requests. It will ensure that all requests for a specific Squid request will be executed in the same thread. The main interpreter will not be used if a thread pool is enabled.

* `service_thread_init_script`: expects a path to a Tcl script. If a thread pool will be used (`threads_number > 0`), this script will be used to initialise the interpreter in each thread.

* `service_thread_retire_script`: expects a path to a Tcl script. If a thread pool will be used (`threads_number > 0`), this script will be called before a thread is terminated. (Right now the threads are not terminated and this script will never be called).

### The [library file](library/ecap-tcl.in): ecap-tcl.tcl

Explain what these tests test and why

```
Give an example
```

### And coding style tests

Explain what these tests test and why

```
Give an example
```

## Deployment

Add additional notes about how to deploy this on a live system

## Version

The current version of ecap-tcl is: 0.1 (beta)
This software is of beta quality. 

## Authors

* **Georgios Petasis** - *Initial work* - [Home Page](http://www.ellogon.org/petasis/)

See also the list of [contributors](https://github.com/your/project/contributors) who participated in this project.

## License

This project is licensed under the BSD License - see the [license.terms](license.terms) file for details.

## Acknowledgments

* Based on the [sample adapter](http://www.e-cap.org/Downloads) of the [eCAP library](http://www.e-cap.org/).
