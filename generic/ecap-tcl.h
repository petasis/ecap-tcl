#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#define HAVE_CONFIG_H
#include <libecap/common/libecap.h>
#include <libecap/common/registry.h>
#include <libecap/common/errors.h>
#include <libecap/common/message.h>
#include <libecap/common/header.h>
#include <libecap/common/names.h>
#include <libecap/common/named_values.h>
#include <libecap/host/host.h>
#include <libecap/adapter/service.h>
#include <libecap/adapter/xaction.h>
#include <libecap/host/xaction.h>

#include <tcl.h>
#include "tpool.h"
#include "cmds.h"

//#if LIBECAP_VERSION == 1.0.0 || LIBECAP_VERSION == 1.0.1
//  #define HAVE_ECAP_VERSION 100
//#else
//  #define HAVE_ECAP_VERSION 20
//#endif
#define HAVE_ECAP_VERSION 100

namespace Adapter { // not required, but adds clarity

using libecap::size_type;

class Xaction;

class Service: public libecap::adapter::Service {
  public:
    // About
    virtual std::string uri() const; // unique across all vendors
    virtual std::string tag() const; // changes with version and config
    virtual void describe(std::ostream &os) const; // free-format info

    // Configuration
    virtual void configure(const libecap::Options &cfg);
    virtual void reconfigure(const libecap::Options &cfg);
    void setOne(const libecap::Name &name, const libecap::Area &valArea);

    // Lifecycle
    virtual void start();  // expect makeXaction() calls
    virtual void stop();   // no more makeXaction() calls until start()
    virtual void retire(); // no more makeXaction() calls

    // Scope (XXX: this may be changed to look at the whole header)
    virtual bool wantsUrl(const char *url) const;

    // Work
#if HAVE_ECAP_VERSION >= 100
    virtual MadeXactionPointer makeXaction(libecap::host::Xaction *hostx);
#else
    virtual libecap::adapter::Xaction *makeXaction(libecap::host::Xaction *hostx);
#endif

  public:
    // Configuration storage
    std::string service_init_script;
    std::string service_start_script;
    std::string service_stop_script;
    std::string service_retire_script;
    std::string service_thread_init_script;
    std::string service_thread_retire_script;
    std::string threads_number;
    std::string victim;        // the text we want to replace
    std::string replacement;   // what the replace the victim with


    int  actionStart(Xaction *action) const;
    int  actionStop(Xaction *action) const;
    int  contentAdapt(Xaction *action,
                      std::string &chunk) const; // converts vb to ab
    int  contentDone(Xaction *action, bool atEnd, std::string &chunk) const;

  protected:
    void setThreadsNumber(const std::string &value);
    void initPool(void);
    void freePool(void);
    void setVictim(const std::string &value);
    void evalScript(const std::string &path);
  private:
    unsigned int nthread = 0;   // Number of threads
    TPool *pool = NULL;         // Thread pool
    mutable
    TPoolThread *thread = NULL; // The thread associated with this service
    mutable unsigned int size = 0;
};

#define ACTION_TOKEN_SIZE 2*sizeof(void*)+4
class Xaction: public libecap::adapter::Xaction {
  public:
    Xaction(libecap::shared_ptr<Service> s, libecap::host::Xaction *x);
    virtual ~Xaction();

    // meta-information for the host transaction
    virtual const libecap::Area option(const libecap::Name &name) const;
    virtual void visitEachOption(libecap::NamedValueVisitor &visitor) const;

    // lifecycle
    virtual void start();
    virtual void stop();

    // adapted body transmission control
    virtual void abDiscard();
    virtual void abMake();
    virtual void abMakeMore();
    virtual void abStopMaking();

    // adapted body content extraction and consumption
    virtual libecap::Area abContent(size_type offset, size_type size);
    virtual void abContentShift(size_type size);

    // virgin body state notification
    virtual void noteVbContentDone(bool atEnd);
    virtual void noteVbContentAvailable();

    // libecap::Callable API, via libecap::host::Xaction
    virtual bool callable() const;

    libecap::host::Xaction *host() const;

    char token[ACTION_TOKEN_SIZE];

  protected:
    void stopVb(); // stops receiving vb (if we are receiving it)
    libecap::host::Xaction *lastHostCall(); // clears hostx

  private:
    libecap::shared_ptr<const Service> service; // configuration access
    libecap::host::Xaction *hostx; // Host transaction rep

    std::string buffer; // for content adaptation

    typedef enum { opUndecided, opOn, opComplete, opNever } OperationState;
    OperationState receivingVb;
    OperationState sendingAb;
    bool tcl_action_start = false;
};

enum TclObjMethod   { string, bytes, bytearray, boolean };
enum TclResultValue { result_string, result_boolean };

typedef struct _TclCallClientData {
  unsigned int   objc;
  const char    *token[3];
  TclObjMethod   init[3];
  size_t         size[3];
  int            code;
  std::string    result;
  int            result_boolean = false;
  TclResultValue expects = result_string;

  Xaction      *action;
} TclCallClientData;

} // namespace Adapter
