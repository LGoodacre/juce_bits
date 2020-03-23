/*
  ==============================================================================

    ArgumentOrganizer.h
    Created: 21 Mar 2020 6:12:04pm
    Author:  L.Goodacre

    MIT License

    Copyright (c) 2020 Liam Goodacre

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

    Requires C++ 17

  ==============================================================================
*/

#pragma once
#include "../JuceLibraryCode/JuceHeader.h"
#include <utility>
#include <tuple>
#include <type_traits>



// A simple union class that lets ArgumentOrganizer gather ints, doubles and int64s together
// Give this a Type in an ArgumentOrganizer and it should collect any numeric type.

struct AnyNumber
{
    AnyNumber(int    i) : v(i) {}
    AnyNumber(double d) : v(d) {}
    AnyNumber(int64  i) : v(i) {}
    AnyNumber()         : v(var()) {}

    operator int()    { return v; }
    operator double() { return v; }
    operator int64 () { return v; }

    operator int()    const { return v; }
    operator double() const { return v; }
    operator int64 () const { return v; }

private:
    var v;
};



// A VariantConverter so that ArgumentOrganizer can work with DynamicObject::Ptr's

template <>
struct VariantConverter<DynamicObject::Ptr>
{
    static DynamicObject::Ptr fromVar(const var& v) { return { v.getDynamicObject() }; }
    static var toVar(const DynamicObject::Ptr& p) { return &p; }
};



// A VariantConverter so that ArgumentOrganizer can work with AnyNumbers

template <>
struct VariantConverter<AnyNumber>
{
    static double fromVar(const var& v)  { return double(v); }
    static var toVar(const AnyNumber& d) { return double(d); }
};



// ArgumentOrganizer will match the received arguments according to the type specified in the template parameter.
// If the specified type is wrong or is out of range, it will return a default value of that type.

template<typename... T>
class ArgumentOrganizer
{
public:
    ArgumentOrganizer(const var::NativeFunctionArgs& a) :
        thisObject(a.thisObject.getDynamicObject()),
        tpl(populate_tuple<T...>(a, std::make_index_sequence<sizeof...(T)>{})) {}


    // Fetch the nth argument. The return type corresponds to the nth term in the template parameter of the ArgumentOrganizer.
    // If the nth argument doesn't match the specified type, you will get a default value.
    // (Exception: if the argument was numeric and the type is String, you will get a String version of the number).
    // If you try to fetch an argument that you didn't specify as a parameter, you get a compile error.
    
    template <int n>
    auto getArgument() const
    { return std::get<n>(tpl); }


    // Dynamic cast a DynamicObject or ReferenceCountedObject to a given type.
    
    template <class C, int i>
    auto castObject() const
    { return dynamic_cast<C*> (getArgument<i>().get()); }

private:
    DynamicObject::Ptr thisObject;
    const std::tuple<T...> tpl;

    template<typename... Args, int... i>
    auto populate_tuple(const var::NativeFunctionArgs& a, const std::index_sequence<i...>&)
    { return std::make_tuple((VariantConverter<Args>::fromVar(i < a.numArguments ? a.arguments[i] : var()))...); }
};


// ComplexArgumentOrganizer gathers arguments in an array, according to the type specifid in the parameter.
// If the specified type is out of range, it will return a default value of that type.
// It's more useful, but also slower than ArgumentOrganizer.

template <typename... T>
class ComplexArgumentOrganizer
{
public:
    ComplexArgumentOrganizer(const var::NativeFunctionArgs& a) :
        thisObject(a.thisObject.getDynamicObject()),
        tpl(populate_complex_tuple(a, tpl)) {}


    // Fetch the mth instance of the nth type of argument.
    
    template<int n>
    auto getArgument(const int m) const
    { return std::get<n>(tpl)[m]; }


    // Dynamic cast the mth instance of the nth type to a specified type.
    
    template <class C, int i>
    auto castObject(const int j) const
    { return dynamic_cast<C*> (getArgument<i>(j).get()); }


    // Get the number of arguments of the nth type.
    
    template <int n>
    int getNumArguments() const
    { return std::get<n>(tpl).size(); }


    // Fetch a const reference to the array of the nth type, for iteration, etc.
    
    template <int n>
    const auto& getConstArray() const
    { return std::get<n>(tpl); }


    // Fetch a reference to the array of the nth type, for iteration, etc.
    
    template <int n>
    auto& getArray()
    { return std::get<n>(tpl); }

private:
    DynamicObject::Ptr thisObject;
    const std::tuple<Array<T>...> tpl;

    template<typename... A>
    auto populate_complex_tuple(const var::NativeFunctionArgs& a, const std::tuple<Array<A>...>& t)
    { return std::make_tuple(Array<A>(populate_array<A>(a))...); }

    template<typename T>
    auto populate_array(const var::NativeFunctionArgs& vArguments)
    {
        Array<T> arrayToReturn;

        for (int i = 0; i < vArguments.numArguments; ++i)
        {
            const auto element = vArguments.arguments[i];

            if (varIsSameTypeAs<T>(element))
                arrayToReturn.add(VariantConverter<T>::fromVar(element));
        }

        return arrayToReturn;
    }
    // If anyone can figure out a better way of matching vars by type, let me know!
    template <typename T>
    bool varIsSameTypeAs(const var& v)
    {
        if (std::is_same<T, String>::value)
            return v.isString();

        else if (std::is_same<T, AnyNumber>::value)
            return v.isInt() || v.isDouble() || v.isInt64();

        else if (std::is_same<T, int>::value)
            return v.isInt();

        else if (std::is_same<T, bool>::value)
            return v.isBool();

        else if (std::is_same<T, double>::value)
            return v.isDouble();

        else if (std::is_same<T, int64>::value)
            return v.isInt64();

        else if (std::is_same<T, Array<var>>::value)
            return v.isArray();

        else if (std::is_same<T, DynamicObject::Ptr>::value)
            return v.isObject();

        else return false;
    }
};
