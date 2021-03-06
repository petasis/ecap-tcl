#############################################################################
#############################################################################
###
###   ecap-tcl.tcl
### -------------------------------------------------------------------------
###  The library of the ecap-tcl package.
###
#############################################################################
#############################################################################


namespace eval ::ecap-tcl {

  ## If the brotli package is available, use it...
  catch {
    package require brotli
    set brotli [::brotli new]
  }

  namespace eval tcloo {
    variable client_objects {}

    proc register {client} {
      variable client_objects
      if {![info object class $client ::ecap-tcl::AbstractProcessor]} {
        error "client must be a supeclass of ::ecap-tcl::AbstractProcessor"
      }
      foreach type [$client mime-types] {
        dict set client_objects $type $client
      }
    };# register

    proc unregister {client} {
      variable client_objects
      foreach type [$client mime-types] {
        if {[dict get $client_objects [string tolower $type]] eq $client} {
          dict unset client_objects $type
        }
      }
    };# unregister

    proc call_client {action token args} {
      variable client_objects
      if {![::ecap-tcl::action header exists Content-Type]} {
        return -code break
      }
      lassign [split [::ecap-tcl::action header get Content-Type] \;] \
              mime params
      set mime [string trim $mime]; set params [string trim $params]
      ## Do we have a processor for this mime type?
      if {[dict exists $client_objects "$mime; $params"]} {
        set client [dict get $client_objects "$mime; $params"]
      } elseif {[dict exists $client_objects $mime]} {
        set client [dict get $client_objects $mime]
      } else {
        return -code break
      }
      try {
        $client $action $token $mime $params {*}$args
      } on continue {result options} {
        return -options $options $result
      } on break {result options} {
        return -options $options $result
      } on ok {result options} {
        return -options $options $result
      } on error {result options} {
        # puts "Error: \"$result\""
        # puts "  $::errorInfo"
        return -options $options $result
      }
    };# call_client

    ## NOT USED
    proc call_client_no_action {action args} {
      variable client_objects
      ## Iterate over all clients. If one wants it, return true...
      dict for {mime client} $client_objects {
        try {
          $client $action {*}$args
        } on continue {result options} {
          return -code continue
        } on break {result options} {
          ## Proceed to the next client
        } on ok {result options} {
          return $result
        } on error {result options} {
          # puts "Error: \"$result\""
          # puts "  $::errorInfo"
          ## Proceed to the next client
        }
      }
      return true
    };# call_client_no_action

    proc wantsUrl {url} {
      variable client_objects
      ## Iterate over all clients. If one wants it, return true...
      dict for {mime client} $client_objects {
        if {![catch {$client onWantsUrl $url} result] && $result} {
          return 1
        }
      }
      return 0
    };# wantsUrl

  };# namespace tcloo

  proc wantsUrl {url} {
    tcloo::wantsUrl $url
  };# wantsUrl

  proc actionStart {token} {
    tcloo::call_client onActionStart $token
  };# actionStart

  proc actionStop {token} {
    tcloo::call_client onActionStop $token
  };# actionStop

  proc contentAdapt {token chunk} {
    tcloo::call_client onContentAdapt $token $chunk
    # return -code continue | break;  # Do not modify chunk
    # return {}; # Empty response
  };# contentAdapt

  proc contentDone {token atEnd} {
    tcloo::call_client onContentDone $token $atEnd
    # return -code continue | break; # Do not append anything at the end
    #                                  of the content
    # any other value, will be appended at the returned result.
  };# contentDone

};# namespace ::ecap-tcl

############################################################################
## The following classes define a set of processors.
## ::ecap-tcl::SampleHTMLProcessor is such an example. In order to play with it,
## simply create an instance of the ::ecap-tcl::SampleHTMLProcessor class.
############################################################################

oo::class create ::ecap-tcl::AbstractProcessor {
  constructor {} {
    my register
  };# constructor

  destructor {
    my unregister
  };# destructor

  method mime-types {} {
    error "abstract class"
  };# mime-types

  method onWantsUrl {url} {
    return true
  };# onWantsUrl

  method onActionStart  {token mime params} {
    return -code break
  };# onActionStart

  method onActionStop   {token mime params} {
    return -code break
  };# onActionStart

  method onContentAdapt {token mime params chunk} {
    return -code break
  };# onContentAdapt

  method onContentDone  {token mime params atEnd} {
    return -code break
  };# onContentDone

  method register {} {
    ::ecap-tcl::tcloo::register [self object]
  };# register

  method unregister {} {
    ::ecap-tcl::tcloo::unregister [self object]
  };# unregister

  method printHeaders {{channel stdout}} {
    dict for {k v} [::ecap-tcl::action header get] {
      puts $channel "  $k: \"$v\""
    }
    flush $channel
  };# printHeaders

  method setContentLength {data} {
    ::ecap-tcl::action header set Content-Length \
      [::ecap-tcl::action content bytelength $data]
    return $data
  };# setContentLength

};# ::ecap-tcl::AbstractProcessor

oo::class create ::ecap-tcl::ContentProcessor {
  superclass ::ecap-tcl::AbstractProcessor

  method onActionStart {token mime params} {
    my variable content_action request_uri
    dict set content_action $token {}
    set request_uri [::ecap-tcl::action client request_uri]
  };# onActionStart

  method onActionStop {token mime params} {
    my variable content_action
    catch {dict unset content_action $token}
  };# onActionStart

  method onContentAdapt {token mime params chunk} {
    my variable content_action
    dict append content_action $token $chunk
    return {}
  };# onContentAdapt

  method onContentDone {token mime params atEnd} {
    if {!$atEnd} {return -code continue}
    my processContent $token $mime $params
  };# onContentDone

  method processContent {token mime params} {
    ## Variable content_action contains the data
    my variable content_action
    dict get $content_action $token
  };# processContent

};# class ::ecap-tcl::ContentProcessor

oo::class create ::ecap-tcl::UncompressProcessor {
  superclass ::ecap-tcl::ContentProcessor

  method onActionStart {token mime params} {
    next $token $mime $params
    my variable content_is_compressed content_encoding
    ## Check the Content-Encoding header...
    if {[::ecap-tcl::action header exists Content-Encoding]} {
      switch -- [string tolower \
                  [::ecap-tcl::action header get Content-Encoding]] {
        br      {dict set content_is_compressed $token br}
        gzip    {dict set content_is_compressed $token gzip}
        deflate {dict set content_is_compressed $token deflate}
        default {dict set content_is_compressed $token no}
      }
    } else {
      dict set content_is_compressed $token no
    }
  };# onActionStart

  method onActionStop {token mime params} {
    my variable content_is_compressed content_uncompressed \
                content_compression_header
    catch {dict unset content_uncompressed $token}
    catch {dict unset content_is_compressed $token}
    catch {dict unset content_compression_header $token}
    next $token $mime $params
  };# onActionStart

  method onContentDone {token mime params atEnd} {
    if {!$atEnd} {return -code continue}
    if {[catch {my uncompressContent $token $mime $params} error]} {
      puts "Decompression error: $error"
      ## Return the original content. We failed to decompress it...
      my variable content_action
      return [dict get $content_action $token]
    }
    my processContentAndCompress $token $mime $params
  };# onContentDone

  method processContentAndCompress {token mime params} {
    my variable content_is_compressed
    my compressContent $token [my processContent $token $mime $params] \
                                [dict get $content_is_compressed $token]
  };# processContentAndCompress

  method uncompressContent {token mime params} {
    my variable content_is_compressed content_uncompressed \
                content_action content_compression_header
    dict set content_uncompressed $token {}
    set data [dict get $content_action $token]
    switch [dict get $content_is_compressed $token] {
      no      {dict set content_uncompressed $token $data}
      br      {
        if {[catch {
          dict set content_uncompressed $token \
                    [${::ecap-tcl::brotli} decompress $data]
        }]} {
          dict set content_uncompressed $token $data
        }
      }
      gzip    {
        dict set content_uncompressed $token \
                    [zlib gunzip $data -headerVar compression_header]
        dict set content_compression_header $token $compression_header
      }
      deflate {dict set content_uncompressed $token \
                    [zlib inflate $data]}
    }
  };# uncompressContent

  method compressContent {token bindata {method gzip}} {
    # puts compressContent:[tcl::unsupported::representation $bindata]
    switch -- $method {
      gzip {
        my variable content_compression_header
        ::ecap-tcl::action header set Content-Encoding gzip
        set data [zlib gzip $bindata -header \
                       [dict get $content_compression_header $token]]
      }
      br {
        if {[catch {${::ecap-tcl::brotli} compress $bindata} data]} {
          ::ecap-tcl::action header set Content-Encoding deflate
          set data [zlib deflate $bindata]
        } else {
          ::ecap-tcl::action header set Content-Encoding br
        }
      }
      deflate {
        ::ecap-tcl::action header set Content-Encoding deflate
        set data [zlib deflate $bindata]
      }
      no {
        ::ecap-tcl::action header remove Content-Encoding
        set data $bindata
      }
      default {error "unsupported compression method \"$method\""}
    }
    my setContentLength $data
  };# compressContent

};# class ::ecap-tcl::UncompressProcessor

oo::class create ::ecap-tcl::TextProcessor {
  superclass ::ecap-tcl::UncompressProcessor

  method onActionStop {token mime params} {
    my variable content_encoding
    catch {dict unset content_encoding $token}
    next $token $mime $params
  };# onActionStart

  method uncompressContent {token mime params} {
    next $token $mime $params
    my variable content_encoding content_uncompressed
    ## Convert the data to utf-8, from the specified content_encoding
    dict set content_encoding $token iso8859-1
    set params [split [string map {{ } {}} [string tolower $params]] =]
    if {[dict exists $params charset]} {
      set iata_content_encoding [string map {
        iso-8859 iso8859 windows-12 cp12
      } [dict get $params charset]]
      switch -- $iata_content_encoding {
        default {dict set content_encoding $token $iata_content_encoding}
      }
    } else {
      ## Maybe we can locate encoding in the content?
      set match [regexp -nocase -inline {<meta(?!\s*(?:name|value)\s*=)(?:[^>]*?content\s*=[\s\"']*)?([^>]*?)[\s\"';]*charset\s*=[\s\"']*([^\s\"'/>]*)} [dict get $content_uncompressed $token]]
      lassign $match _ type iata_content_encoding
      if {$iata_content_encoding ne ""} {
        set iata_content_encoding [string map {
          iso-8859 iso8859 windows-12 cp12
        } [string tolower $iata_content_encoding]]
        switch -- $iata_content_encoding {
          default {dict set content_encoding $token $iata_content_encoding}
        }
      }
    }
    # puts "==>> [dict get $content_encoding $token]"
    dict set content_uncompressed $token \
        [encoding convertfrom [dict get $content_encoding $token] \
             [dict get $content_uncompressed $token]]
  };# uncompressContent

  method compressContent {token data {method gzip}} {
    # puts compressContent:[tcl::unsupported::representation $data]
    my variable content_encoding
    next $token [encoding convertto [dict get $content_encoding $token] \
                                    $data] $method
  };# compressContent

};# class ::ecap-tcl::TextProcessor

oo::class create ::ecap-tcl::SampleHTMLProcessor {
  superclass ::ecap-tcl::TextProcessor

  method mime-types {} {
    return {text/html}
  };# mime-types

  method processContent {token mime params} {
    my variable content_uncompressed content_action request_uri
    ::ecap-tcl::action header add X-Ecap \
      [::ecap-tcl::action host uri]
    my printHeaders
    puts "URL: \"$request_uri\""
    puts "Token: $token, Data:\
        \"[string range [dict get $content_uncompressed $token] 0 30]\"..."
    dict get $content_action $token
  };# processContent

};# class ::ecap-tcl::SampleHTMLProcessor

package provide ecap-tcl 0.3

## Test:
#  ::ecap-tcl::SampleHTMLProcessor create processor
