# ecap-tcl
An eCAP adapter for Tcl.
[What is eCAP?](http://www.e-cap.org/) - eCAP is a software interface that allows a network application, such as an HTTP proxy or an ICAP server, to outsource content analysis and adaptation to a loadable module.

For each applicable protocol message being processed, an eCAP-enabled host application supplies the message details to the adaptation module and gets back an adapted message, a "not interested" response, or a "block this message now!" instruction. These exchanges often include message bodies.

[What is ecap-tcl?](https://github.com/petasis/ecap-tcl) - It is an eCAP module that can be loaded into an eCAP host, and directs the messages to [Tcl](https://tcl.tk/), allowing to write adaptation modules in the [Tcl language](https://tcl.tk/).

## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes. See deployment for notes on how to deploy the project on a live system.

Please note that this software has been tested only with [Squid](http://www.squid-cache.org/) host, and Tcl 8.6. Success under other host applications and Tcl versions may vary. Also, this is beta quality software.

### Prerequisites

* [Tcl 8.6](http://www.tcl.tk/software/tcltk/8.6.html) - The Tcl language. Tcl 8.6+ may be available as a package in your Linux distribution. ecap-tcl has been tested with ActiveTcl 8.6.4. (Why is Tcl 8.6 required? The C/C++ part of the adapter has no specific requirements on the version. This requirement comes from the [library file](library/ecap-tcl.tcl.in), which uses TclOO, the `dict`, `try` and `lassign` commands.)

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

### The [library file](library/ecap-tcl.tcl.in): ecap-tcl.tcl

The library file **is not automatically loaded** when the ecap-tcl adapter is loaded into the host application. It must be **explicitely loaded** by the Squid configuration (i.e. loaded by one of the scripts specified).

In the provided sample scripts, it is loaded by the [library/sample_scripts/service_thread_init.tcl](library/sample_scripts/service_thread_init.tcl) script.

#### What is needed from the library file?

The ecap-tcl adapter, will receive 5 event types from the host application, and will call 5 Tcl commands to process them. These commands and their parameters are:

* `::ecap-tcl::wantsUrl <url>` - This command will be called to decide whether the request with this `url` is wanted. The command must respond with `true` or `false`. In case of an error, or if the command does not exist, `true` is assumed. No other information beyond the <url> is available.

* `::ecap-tcl::actionStart <token>` - This command will be called to notify that the processing for this `token` will start. Usually, this command will initialise a state for this `token`. (`token` is an identifier for a specific request done on the host application.)

* `::ecap-tcl::contentAdapt <token> <chunk>` - This command will be called to process a **piece** of the content, from the body of the message retrieved by the host application, in order to fulfil the request). This command is expected to return the modified version of the `chunk`. This command may accumulate all chunks (i.e. by appending them to a Tcl variable). In such a case, it can return `{}`, so nothing is returned to the host application.

* `::ecap-tcl::contentDone <token> <atEnd>` - This command will be called after all chunks have been processed with `::ecap-tcl::contentAdapt`. The return value of this command will be returned to the host application as content. If chunks have been accumulated, the adapted content can be returned by this command. It will be appended to the content returned by the `::ecap-tcl::contentAdapt` calls.

* `::ecap-tcl::actionStop <token>` - This command will be called to signal that processing of the message represented by `token` has been finished, and allocated resources must be freed.

These are the 5 commands that are expected by the ecap-tcl adapter.
During the execution of the last 4 commands, the command `::ecap-tcl::action` will be available, which can be used to request/modify/remove headers of the request message.

#### What else is defined in the library file?

A number of TclOO classes, to facilitate usage. This library section is oriented towards processing textual content, with the main class being `::ecap-tcl::TextProcessor`. This class will accumulate all chunks (in the variable `content_uncompressed`), and in case of compressed content, it will be decompressed first. (The original content as received is always available in the variable `content_action`.) This class can be sub-classed, to easily adapt content.

An example is class ::ecap-tcl::SampleHTMLProcessor. It is called when the mime type is `text/html`, and in its content adaptation method (`processContent`), it adds the `X-Ecap` header, it prints all message headers (after the addition), it prints the message url, the token, and the first 30 characters of the message body. It returns the unmodified, original message body back.

```
oo::class create ::ecap-tcl::SampleHTMLProcessor {
  superclass ::ecap-tcl::TextProcessor

  method mime-types {} {
    return {text/html}
  };# mime-types

  method processContent {token mime params} {
    my variable content_uncompressed content_action request_url
    ::ecap-tcl::action header add X-Ecap \
      [::ecap-tcl::action host uri]
    my printHeaders
    puts "URL: \"$request_url\""
    puts "Token: $token, Data:\
        \"[string range [dict get $content_uncompressed $token] 0 30]\"..."
    dict get $content_action $token
  };# processContent

};# class ::ecap-tcl::SampleHTMLProcessor
```

The script [library/sample_scripts/service_thread_init.tcl](library/sample_scripts/service_thread_init.tcl) simply enables it with:
```
::ecap-tcl::SampleHTMLProcessor create processor
```

A sample output, when the request to Squid is `http://www.google.gr` is:
```
  Date: "Sun, 11 Dec 2016 14:07:40 GMT"
  Expires: "-1"
  Cache-Control: "private, max-age=0"
  Content-Type: "text/html; charset=ISO-8859-7"
  P3P: "CP="This is not a P3P policy! See https://www.google.com/support/accounts/answer/151657?hl=en for more info.""
  Server: "gws"
  X-XSS-Protection: "1; mode=block"
  X-Frame-Options: "SAMEORIGIN"
  Set-Cookie: "NID=91=QgZYLsGxxNYYq4QLqh1yIoCU-uj22C3tBoNWkFqFuFPru6NlsqJv2i7R0_wDwyAG9opD_SgKu9_8Wg5diTO8Yk33oaMAvfpaNsM1x_1TxSMKX2Mi_BUXYB94Po_UE5DO; expires=Mon, 12-Jun-2017 14:07:40 GMT; path=/; domain=.google.gr; HttpOnly"
  Accept-Ranges: "none"
  Vary: "Accept-Encoding"
  Transfer-Encoding: "chunked"
  X-Ecap: "ecap://squid-cache.org/ecap/hosts/squid"
URL: "/"
Token: _d0f9267b29560000_, Data: "<!doctype html><html itemscope="...
```

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
