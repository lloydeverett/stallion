#include <iostream>
#include <string>

#include "avarice.hpp"

class MyObjectT {
public:
    virtual ~MyObjectT() = default;
};

using av = avarice<MyObjectT>;

class MyObject : public MyObjectT {
    std::string str;
public:
    MyObject(std::string str) : str(std::move(str)) { }
};

int main() {
    struct CommitRefState {
        std::string x;
        void emplace(av::Emplacer<MyObject> emplacer) const {
            emplacer("foo");
            //  TODO: Implement
        }
        void traverse() const & {
            //  TODO: Implement
        }
        void traverse() && {
            //  TODO: Implement
        }
    } state;

    av::RefTo<MyObject> a { av::shared_ref_type<MyObject, CommitRefState>, state };
    a.resolve();

    std::cout << sizeof(av::RefTo<MyObject>) << std::endl;
    std::cout << alignof(av::ThreadLocalRef<MyObject, CommitRefState>) << std::endl;
    std::cout << alignof(std::max_align_t) << std::endl;

    av::RefTo<MyObject> a_copy = a;

    av::Ref b             { av::copying_ref_type<MyObject, CommitRefState>, state };
    b.resolve();

    av::Ref b_copy {b};

    av::Ref aa { a.decay() };
    aa.resolve();

    av::RefTo<MyObject> aaa { aa.undecay<MyObject>() };
    aaa.resolve();

    a.resolve();
    aa.resolve();

    std::cout << sizeof(av::Ref) << std::endl;

    return 0;
}

