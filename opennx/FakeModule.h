#ifndef _FAKEMODULARITY_H_
#define _FAKEMODULARITY_H_
#include <memory>


class FakeModule {
private:
    FakeModule();
    std::auto_ptr<wxFileName> module;
    FakeModule(FakeModule const&) {} //delete
    FakeModule& operator= (FakeModule const&) {} //delete
public:
    static wxString fileName;

    static FakeModule& instance();
    bool exists();
};
#endif
