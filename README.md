# ecap-tcl
An eCAP adapter for Tcl.
[What is eCAP?](http://www.e-cap.org/) - eCAP is a software interface that allows a network application, such as an HTTP proxy or an ICAP server, to outsource content analysis and adaptation to a loadable module.

For each applicable protocol message being processed, an eCAP-enabled host application supplies the message details to the adaptation module and gets back an adapted message, a "not interested" response, or a "block this message now!" instruction. These exchanges often include message bodies.

[What is ecap-tcl?](https://github.com/petasis/ecap-tcl) - It is an eCAP module that can be loaded into an eCAP host, and directs the messages to [Tcl](https://tcl.tk/), allowing to write adaptation modules in the [Tcl language](https://tcl.tk/).

## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes. See deployment for notes on how to deploy the project on a live system.

Please note that this software has been tested only with [Squid](http://www.squid-cache.org/) host, and Tcl 8.6. Success under other host applications and Tcl versions may vary. Also, this is beta quality software.

### Prerequisites

* [Tcl 8.6](http://www.tcl.tk/software/tcltk/8.6.html) - The Tcl language. Tcl 8.6+ may be available as a package in your Linux distribution. ecap-tcl has been tested with ActiveTcl 8.6.4.

* [libecap 1.0.0](http://www.e-cap.org/Downloads) - The eCAP library. ecap-tcl has been tested with version 1.0.0 (it may work with either more recent or earlier versions). ecap-tcl has been based on the adapter sample version 1.0.0. Some Linux distributions may have this library as a package:
  * Fedora 24: dnf install libecap-devel

* A standard UNIX build chain, autoconf, make, g++. (ecap-tcl is written in C++).

```
Give examples
```

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

## Running the tests

Explain how to run the automated tests for this system

### Break down into end to end tests

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

## Built With

* [Dropwizard](http://www.dropwizard.io/1.0.2/docs/) - The web framework used
* [Maven](https://maven.apache.org/) - Dependency Management
* [ROME](https://rometools.github.io/rome/) - Used to generate RSS Feeds

## Contributing

Please read [CONTRIBUTING.md](https://gist.github.com/PurpleBooth/b24679402957c63ec426) for details on our code of conduct, and the process for submitting pull requests to us.

## Versioning

We use [SemVer](http://semver.org/) for versioning. For the versions available, see the [tags on this repository](https://github.com/your/project/tags). 

## Authors

* **Georgios Petasis** - *Initial work* - [Home Page](http://www.ellogon.org/petasis/)

See also the list of [contributors](https://github.com/your/project/contributors) who participated in this project.

## License

This project is licensed under the BSD License - see the [license.terms](license.terms) file for details.

## Acknowledgments

* Based on the [sample adapter](http://www.e-cap.org/Downloads) of the [eCAP library](http://www.e-cap.org/).
