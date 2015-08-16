#pragma once

#include "mppacker.hpp"
#include <functional>

namespace mpcompact {

class Object
{
    Object(const Object&) = delete;
    void operator=(const Object&) = delete;

protected:
    struct Field
    {
        std::function<void(Packer&)>    f_pack;
        std::function<void(Unpacker&)>  f_unpack;
        Object* nestedObject;
    };

    Object*             parentObj;
    std::vector<Field>  fieldVec;

public:
    Object() : parentObj(NULL), fieldVec() {}
    virtual ~Object() {}

    void inherit(Object* p)
    {
        parentObj = p;
    }

    template<typename T>
    typename std::enable_if<!std::is_base_of<Object, T>::value, Object&>::type
    reg(T& arg)
    {
        fieldVec.emplace_back(
            Field({
                [&](Packer& packer)     { packer.pack(arg);     },
                [&](Unpacker& unpacker) { unpacker.unpack(arg); },
                NULL
            }));
        return *this;
    }

    Object& reg(Object& object)
    {
        fieldVec.emplace_back(
            Field({
                NULL,
                NULL,
                &object
            }));

        return *this;
    }

    const Object& pack(Packer& packer) const
    {
        if(parentObj)
            parentObj->pack(packer);

        for(auto& field : fieldVec) {
            if(field.nestedObject == NULL)
                field.f_pack(packer);
            else
                field.nestedObject->pack(packer);
        }

        return *this;
    }

    const Object& unpack(Unpacker& unpacker) const
    {
        if(parentObj)
            parentObj->unpack(unpacker);

        for(auto& field : fieldVec) {
            if(field.nestedObject == NULL)
                field.f_unpack(unpacker);
            else
                field.nestedObject->unpack(unpacker);
        }

        return *this;
    }
};

} // end namespace mpcompact


