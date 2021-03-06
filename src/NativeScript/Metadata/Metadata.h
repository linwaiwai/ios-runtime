//
//  Metadata.h
//  NativeScript
//
//  Created by Ivan Buhov on 8/1/14.
//  Copyright (c) 2014 Telerik. All rights reserved.
//

#ifndef __NativeScript__Metadata__
#define __NativeScript__Metadata__

#include <stack>
#include <string>
#include <type_traits>
#include <vector>

namespace Metadata {

static const int MetaTypeMask = 0b00000111;

template <typename V>
static const V& getProperFunctionFromContainer(const std::vector<V>& container, int argsCount, std::function<int(const V&)> paramsCounter) {
    const V* callee = nullptr;

    for (const V& func : container) {
        auto candidateArgs = paramsCounter(func);
        auto calleeArgs = 0;
        if (candidateArgs == argsCount) {
            callee = &func;
            break;
        } else if (!callee) {
            // no candidates so far, take it whatever it is
            callee = &func;
            calleeArgs = candidateArgs;
        } else if (argsCount < candidateArgs && (calleeArgs < argsCount || candidateArgs < calleeArgs)) {
            // better candidate - looking for the least number of arguments which is more than the amount actually passed
            callee = &func;
            calleeArgs = candidateArgs;
        } else if (calleeArgs < candidateArgs) {
            // better candidate - looking for the maximum number of arguments which less than the amount actually passed (if one with more cannot be found)
            callee = &func;
            calleeArgs = candidateArgs;
        }
    }

    return *callee;
}

inline UInt8 encodeVersion(UInt8 majorVersion, UInt8 minorVersion) {
    return (majorVersion << 3) | minorVersion;
}

inline UInt8 getMajorVersion(UInt8 encodedVersion) {
    return encodedVersion >> 3;
}

inline UInt8 getMinorVersion(UInt8 encodedVersion) {
    return encodedVersion & 0b111;
}

// Bit indices in flags section
enum MetaFlags {
    HasName = 7,
    // IsIosAppExtensionAvailable = 6, the flag exists in metadata generator but we never use it in the runtime
    FunctionReturnsUnmanaged = 3,
    FunctionIsVariadic = 5,
    FunctionOwnsReturnedCocoaObject = 4,
    MemberIsOptional = 0, // Mustn't equal any Method or Property flag since it can be applicable to both
    MethodIsInitializer = 1,
    MethodIsVariadic = 2,
    MethodIsNullTerminatedVariadic = 3,
    MethodOwnsReturnedCocoaObject = 4,
    MethodHasErrorOutParameter = 5,
    PropertyHasGetter = 2,
    PropertyHasSetter = 3,

};

/// This enum describes the possible ObjectiveC entity types.
enum MetaType {
    Undefined = 0,
    Struct = 1,
    Union = 2,
    Function = 3,
    JsCode = 4,
    Var = 5,
    Interface = 6,
    ProtocolType = 7,
    Vector = 8
};

enum MemberType {
    InstanceMethod = 0,
    StaticMethod = 1,
    InstanceProperty = 2,
    StaticProperty = 3
};

enum BinaryTypeEncodingType : Byte {
    VoidEncoding,
    BoolEncoding,
    ShortEncoding,
    UShortEncoding,
    IntEncoding,
    UIntEncoding,
    LongEncoding,
    ULongEncoding,
    LongLongEncoding,
    ULongLongEncoding,
    CharEncoding,
    UCharEncoding,
    UnicharEncoding,
    CharSEncoding,
    CStringEncoding,
    FloatEncoding,
    DoubleEncoding,
    InterfaceDeclarationReference,
    StructDeclarationReference,
    UnionDeclarationReference,
    PointerEncoding,
    VaListEncoding,
    SelectorEncoding,
    ClassEncoding,
    ProtocolEncoding,
    InstanceTypeEncoding,
    IdEncoding,
    ConstantArrayEncoding,
    IncompleteArrayEncoding,
    FunctionPointerEncoding,
    BlockEncoding,
    AnonymousStructEncoding,
    AnonymousUnionEncoding,
    ExtVectorEncoding
};

#pragma pack(push, 1)

template <typename T>
struct PtrTo;
struct Meta;
struct InterfaceMeta;
struct ProtocolMeta;
struct ModuleMeta;
struct LibraryMeta;
struct TypeEncoding;

typedef int32_t ArrayCount;

static const void* offset(const void* from, ptrdiff_t offset) {
    return reinterpret_cast<const char*>(from) + offset;
}

template <typename T>
struct Array {
    class iterator {
    private:
        const T* current;

    public:
        iterator(const T* item)
            : current(item) {
        }
        bool operator==(const iterator& other) const {
            return current == other.current;
        }
        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }
        iterator& operator++() {
            current++;
            return *this;
        }
        iterator operator++(int) {
            iterator tmp(current);
            operator++();
            return tmp;
        }
        const T& operator*() const {
            return *current;
        }
    };

    ArrayCount count;

    const T* first() const {
        return reinterpret_cast<const T*>(&count + 1);
    }

    const T& operator[](int index) const {
        return *(first() + index);
    }

    Array<T>::iterator begin() const {
        return first();
    }

    Array<T>::iterator end() const {
        return first() + count;
    }

    template <typename V>
    const Array<V>& castTo() const {
        return *reinterpret_cast<const Array<V>*>(this);
    }

    int sizeInBytes() const {
        return sizeof(Array<T>) + sizeof(T) * count;
    }

    int binarySearch(std::function<int(const T&)> comparer) const {
        int left = 0, right = count - 1, mid;
        while (left <= right) {
            mid = (right + left) / 2;
            const T& current = (*this)[mid];
            int comparisonResult = comparer(current);
            if (comparisonResult < 0) {
                left = mid + 1;
            } else if (comparisonResult > 0) {
                right = mid - 1;
            } else {
                return mid;
            }
        }
        return -(left + 1);
    }

    int binarySearchLeftmost(std::function<int(const T&)> comparer) const {
        int mid = binarySearch(comparer);
        while (mid > 0 && comparer((*this)[mid - 1]) == 0) {
            mid -= 1;
        }
        return mid;
    }
};

template <typename T>
using ArrayOfPtrTo = Array<PtrTo<T>>;
using String = PtrTo<char>;

struct GlobalTable {
    class iterator {
    private:
        const GlobalTable* _globalTable;
        int _topLevelIndex;
        int _bucketIndex;

        void findNext();

        const Meta* getCurrent();

    public:
        iterator(const GlobalTable* globalTable)
            : iterator(globalTable, 0, 0) {
            findNext();
        }

        iterator(const GlobalTable* globalTable, int32_t topLevelIndex, int32_t bucketIndex)
            : _globalTable(globalTable)
            , _topLevelIndex(topLevelIndex)
            , _bucketIndex(bucketIndex) {
            findNext();
        }

        bool operator==(const iterator& other) const;

        bool operator!=(const iterator& other) const;

        iterator& operator++();

        iterator operator++(int) {
            iterator tmp(_globalTable, _topLevelIndex, _bucketIndex);
            operator++();
            return tmp;
        }

        const Meta* operator*();
    };

    iterator begin() const {
        return iterator(this);
    }

    iterator end() const {
        return iterator(this, this->buckets.count, 0);
    }

    ArrayOfPtrTo<ArrayOfPtrTo<Meta>> buckets;

    const InterfaceMeta* findInterfaceMeta(WTF::StringImpl* identifier) const;

    const InterfaceMeta* findInterfaceMeta(const char* identifierString) const;

    const InterfaceMeta* findInterfaceMeta(const char* identifierString, size_t length, unsigned hash) const;

    const ProtocolMeta* findProtocol(WTF::StringImpl* identifier) const;

    const ProtocolMeta* findProtocol(const char* identifierString) const;

    const ProtocolMeta* findProtocol(const char* identifierString, size_t length, unsigned hash) const;

    const Meta* findMeta(WTF::StringImpl* identifier, bool onlyIfAvailable = true) const;

    const Meta* findMeta(const char* identifierString, bool onlyIfAvailable = true) const;

    const Meta* findMeta(const char* identifierString, size_t length, unsigned hash, bool onlyIfAvailable = true) const;

    int sizeInBytes() const {
        return buckets.sizeInBytes();
    }
};

struct ModuleTable {
    ArrayOfPtrTo<ModuleMeta> modules;

    int sizeInBytes() const {
        return modules.sizeInBytes();
    }
};

struct MetaFile {
private:
    GlobalTable _globalTable;

public:
    static MetaFile* instance();

    static MetaFile* setInstance(void* metadataPtr);

    const GlobalTable* globalTable() const {
        return &this->_globalTable;
    }

    const ModuleTable* topLevelModulesTable() const {
        const GlobalTable* gt = this->globalTable();
        return reinterpret_cast<const ModuleTable*>(offset(gt, gt->sizeInBytes()));
    }

    const void* heap() const {
        const ModuleTable* mt = this->topLevelModulesTable();
        return offset(mt, mt->sizeInBytes());
    }
};

template <typename T>
struct PtrTo {
    int32_t offset;

    bool isNull() const {
        return offset == 0;
    }
    PtrTo<T> operator+(int value) const {
        return add(value);
    }
    const T* operator->() const {
        return valuePtr();
    }
    PtrTo<T> add(int value) const {
        return PtrTo<T>{ .offset = this->offset + value * sizeof(T) };
    }
    PtrTo<T> addBytes(int bytes) const {
        return PtrTo<T>{ .offset = this->offset + bytes };
    }
    template <typename V>
    PtrTo<V>& castTo() const {
        return reinterpret_cast<PtrTo<V>>(this);
    }
    const T* valuePtr() const {
        return isNull() ? nullptr : reinterpret_cast<const T*>(Metadata::offset(MetaFile::instance()->heap(), this->offset));
    }
    const T& value() const {
        return *valuePtr();
    }
};

template <typename T>
struct TypeEncodingsList {
    T count;

    const TypeEncoding* first() const {
        return reinterpret_cast<const TypeEncoding*>(this + 1);
    }
};

union TypeEncodingDetails {
    struct IncompleteArrayDetails {
        const TypeEncoding* getInnerType() const {
            return reinterpret_cast<const TypeEncoding*>(this);
        }
    } incompleteArray;
    struct ConstantArrayDetails {
        int32_t size;
        const TypeEncoding* getInnerType() const {
            return reinterpret_cast<const TypeEncoding*>(this + 1);
        }
    } constantArray;
    struct ExtVectorDetails {
        int32_t size;
        const TypeEncoding* getInnerType() const {
            return reinterpret_cast<const TypeEncoding*>(this + 1);
        }
    } extVector;
    struct DeclarationReferenceDetails {
        String name;
    } declarationReference;
    struct PointerDetails {
        const TypeEncoding* getInnerType() const {
            return reinterpret_cast<const TypeEncoding*>(this);
        }
    } pointer;
    struct BlockDetails {
        TypeEncodingsList<uint8_t> signature;
    } block;
    struct FunctionPointerDetails {
        TypeEncodingsList<uint8_t> signature;
    } functionPointer;
    struct AnonymousRecordDetails {
        uint8_t fieldsCount;
        const String* getFieldNames() const {
            return reinterpret_cast<const String*>(this + 1);
        }
        const TypeEncoding* getFieldsEncodings() const {
            return reinterpret_cast<const TypeEncoding*>(getFieldNames() + this->fieldsCount);
        }
    } anonymousRecord;
};

struct TypeEncoding {
    BinaryTypeEncodingType type;
    TypeEncodingDetails details;

    const TypeEncoding* next() const {
        const TypeEncoding* afterTypePtr = reinterpret_cast<const TypeEncoding*>(offset(this, sizeof(type)));

        switch (this->type) {
        case BinaryTypeEncodingType::ConstantArrayEncoding: {
            return this->details.constantArray.getInnerType()->next();
        }
        case BinaryTypeEncodingType::ExtVectorEncoding: {
            return this->details.extVector.getInnerType()->next();
        }
        case BinaryTypeEncodingType::IncompleteArrayEncoding: {
            return this->details.incompleteArray.getInnerType()->next();
        }
        case BinaryTypeEncodingType::PointerEncoding: {
            return this->details.pointer.getInnerType()->next();
        }
        case BinaryTypeEncodingType::BlockEncoding: {
            const TypeEncoding* current = this->details.block.signature.first();
            for (int i = 0; i < this->details.block.signature.count; i++) {
                current = current->next();
            }
            return current;
        }
        case BinaryTypeEncodingType::FunctionPointerEncoding: {
            const TypeEncoding* current = this->details.functionPointer.signature.first();
            for (int i = 0; i < this->details.functionPointer.signature.count; i++) {
                current = current->next();
            }
            return current;
        }
        case BinaryTypeEncodingType::InterfaceDeclarationReference:
        case BinaryTypeEncodingType::StructDeclarationReference:
        case BinaryTypeEncodingType::UnionDeclarationReference: {
            return reinterpret_cast<const TypeEncoding*>(offset(afterTypePtr, sizeof(TypeEncodingDetails::DeclarationReferenceDetails)));
        }
        case BinaryTypeEncodingType::AnonymousStructEncoding:
        case BinaryTypeEncodingType::AnonymousUnionEncoding: {
            const TypeEncoding* current = this->details.anonymousRecord.getFieldsEncodings();
            for (int i = 0; i < this->details.anonymousRecord.fieldsCount; i++) {
                current = current->next();
            }
            return current;
        }
        default: {
            return afterTypePtr;
        }
        }
    }
};

struct ModuleMeta {
public:
    UInt8 flags;
    String name;
    PtrTo<ArrayOfPtrTo<LibraryMeta>> libraries;

    const char* getName() const {
        return name.valuePtr();
    }

    bool isFramework() const {
        return (flags & 1) > 0;
    }

    bool isSystem() const {
        return (flags & 2) > 0;
    }
};

struct LibraryMeta {
public:
    UInt8 flags;
    String name;

    const char* getName() const {
        return name.valuePtr();
    }

    bool isFramework() const {
        return (flags & 1) > 0;
    }
};

struct JsNameAndName {
    String jsName;
    String name;
};

union MetaNames {
    String name;
    PtrTo<JsNameAndName> names;
};

struct Meta {

private:
    MetaNames _names;
    PtrTo<ModuleMeta> _topLevelModule;
    UInt8 _flags;
    UInt8 _introduced;

public:
    MetaType type() const {
        return (MetaType)(this->_flags & MetaTypeMask);
    }

    const ModuleMeta* topLevelModule() const {
        return this->_topLevelModule.valuePtr();
    }

    bool hasName() const {
        return this->flag(MetaFlags::HasName);
    }

    bool flag(int index) const {
        return (this->_flags & (1 << index)) > 0;
    }

    const char* jsName() const {
        return (this->hasName()) ? this->_names.names->jsName.valuePtr() : this->_names.name.valuePtr();
    }

    const char* name() const {
        return (this->hasName()) ? this->_names.names->name.valuePtr() : this->jsName();
    }

    /**
     * \brief The version number in which this entity was introduced.
     */
    UInt8 introducedIn() const {
        return this->_introduced;
    }

    /**
    * \brief Checks if the specified object is callable
    * from the current device.
    *
    * To be callable, an object must either:
    * > not have platform availability specified;
    * > have been introduced in this or prior version;
    */
    bool isAvailable() const;
};

struct RecordMeta : Meta {

private:
    PtrTo<Array<String>> _fieldsNames;
    PtrTo<TypeEncodingsList<ArrayCount>> _fieldsEncodings;

public:
    const Array<String>& fieldNames() const {
        return _fieldsNames.value();
    }

    size_t fieldsCount() const {
        return fieldNames().count;
    }

    const TypeEncodingsList<ArrayCount>* fieldsEncodings() const {
        return _fieldsEncodings.valuePtr();
    }
};

struct StructMeta : RecordMeta {
};

struct UnionMeta : RecordMeta {
};

struct FunctionMeta : Meta {

private:
    PtrTo<TypeEncodingsList<ArrayCount>> _encoding;

public:
    bool isVariadic() const {
        return this->flag(MetaFlags::FunctionIsVariadic);
    }

    const TypeEncodingsList<ArrayCount>* encodings() const {
        return _encoding.valuePtr();
    }

    bool ownsReturnedCocoaObject() const {
        return this->flag(MetaFlags::FunctionOwnsReturnedCocoaObject);
    }

    bool returnsUnmanaged() const {
        return this->flag(MetaFlags::FunctionReturnsUnmanaged);
    }
};

struct JsCodeMeta : Meta {

private:
    String _jsCode;

public:
    const char* jsCode() const {
        return _jsCode.valuePtr();
    }
};

struct VarMeta : Meta {

private:
    PtrTo<TypeEncoding> _encoding;

public:
    const TypeEncoding* encoding() const {
        return _encoding.valuePtr();
    }
};

struct MemberMeta : Meta {
    bool isOptional() const {
        return this->flag(MetaFlags::MemberIsOptional);
    }
};

struct MethodMeta : MemberMeta {

private:
    PtrTo<TypeEncodingsList<ArrayCount>> _encodings;
    String _constructorTokens;

public:
    bool isVariadic() const {
        return this->flag(MetaFlags::MethodIsVariadic);
    }

    bool isVariadicNullTerminated() const {
        return this->flag(MetaFlags::MethodIsNullTerminatedVariadic);
    }

    bool hasErrorOutParameter() const {
        return this->flag(MetaFlags::MethodHasErrorOutParameter);
    }

    bool isInitializer() const {
        return this->flag(MetaFlags::MethodIsInitializer);
    }

    bool ownsReturnedCocoaObject() const {
        return this->flag(MetaFlags::MethodOwnsReturnedCocoaObject);
    }

    SEL selector() const {
        return sel_registerName(this->selectorAsString());
    }

    // just a more convenient way to get the selector of method
    const char* selectorAsString() const {
        return this->name();
    }

    const TypeEncodingsList<ArrayCount>* encodings() const {
        return this->_encodings.valuePtr();
    }

    const char* constructorTokens() const {
        return this->_constructorTokens.valuePtr();
    }

    bool isImplementedInClass(Class klass, bool isStatic) const;
    bool isAvailableInClass(Class klass, bool isStatic) const {
        return this->isAvailable() && this->isImplementedInClass(klass, isStatic);
    }
};

typedef HashSet<const MemberMeta*> MembersCollection;

std::unordered_map<std::string, MembersCollection> getMetasByJSNames(MembersCollection methods);

struct PropertyMeta : MemberMeta {
    PtrTo<MethodMeta> method1;
    PtrTo<MethodMeta> method2;

public:
    bool hasGetter() const {
        return this->flag(MetaFlags::PropertyHasGetter);
    }

    bool hasSetter() const {
        return this->flag(MetaFlags::PropertyHasSetter);
    }

    const MethodMeta* getter() const {
        return this->hasGetter() ? method1.valuePtr() : nullptr;
    }

    const MethodMeta* setter() const {
        return (this->hasSetter()) ? (this->hasGetter() ? method2.valuePtr() : method1.valuePtr()) : nullptr;
    }

    bool isImplementedInClass(Class klass, bool isStatic) const {
        bool getterAvailable = this->hasGetter() && this->getter()->isImplementedInClass(klass, isStatic);
        bool setterAvailable = this->hasSetter() && this->setter()->isImplementedInClass(klass, isStatic);
        return getterAvailable || setterAvailable;
    }

    bool isAvailableInClass(Class klass, bool isStatic) const {
        return this->isAvailable() && this->isImplementedInClass(klass, isStatic);
    }
};

struct BaseClassMeta : Meta {

    PtrTo<ArrayOfPtrTo<MethodMeta>> instanceMethods;
    PtrTo<ArrayOfPtrTo<MethodMeta>> staticMethods;
    PtrTo<ArrayOfPtrTo<PropertyMeta>> instanceProps;
    PtrTo<ArrayOfPtrTo<PropertyMeta>> staticProps;
    PtrTo<Array<String>> protocols;
    int16_t initializersStartIndex;

    const MemberMeta* member(const char* identifier, size_t length, MemberType type, bool includeProtocols = true, bool onlyIfAvailable = true) const;

    const MethodMeta* member(const char* identifier, size_t length, MemberType type, size_t paramsCount, bool includeProtocols = true, bool onlyIfAvailable = true) const;

    const MembersCollection members(const char* identifier, size_t length, MemberType type, bool includeProtocols = true, bool onlyIfAvailable = true) const;

    const MemberMeta* member(StringImpl* identifier, MemberType type, bool includeProtocols = true) const {
        const char* identif = reinterpret_cast<const char*>(identifier->utf8().data());
        size_t length = (size_t)identifier->length();
        return this->member(identif, length, type, includeProtocols);
    }

    const MethodMeta* member(StringImpl* identifier, MemberType type, size_t paramsCount, bool includeProtocols = true) const {
        const char* identif = reinterpret_cast<const char*>(identifier->utf8().data());
        size_t length = (size_t)identifier->length();
        return this->member(identif, length, type, paramsCount, includeProtocols);
    }

    const MembersCollection members(StringImpl* identifier, MemberType type, bool includeProtocols = true) const {
        const char* identif = reinterpret_cast<const char*>(identifier->characters8());
        size_t length = (size_t)identifier->length();
        return this->members(identif, length, type, includeProtocols);
    }

    const MemberMeta* member(const char* identifier, MemberType type, bool includeProtocols = true) const {
        return this->member(identifier, strlen(identifier), type, includeProtocols);
    }

    /// instance methods

    // Remove all optional methods/properties which are not implemented in the class
    template <typename TMemberMeta>
    static void filterUnavailableMembers(MembersCollection& members, Class klass, bool isStatic) {
        members.removeIf([klass, isStatic](const MemberMeta* memberMeta) {
            return !static_cast<const TMemberMeta*>(memberMeta)->isAvailableInClass(klass, isStatic);
        });
    }

    const MembersCollection getInstanceMethods(StringImpl* identifier, Class klass, bool includeProtocols = true) const {
        MembersCollection methods = this->members(identifier, MemberType::InstanceMethod, includeProtocols);

        filterUnavailableMembers<MethodMeta>(methods, klass, false);

        return methods;
    }

    /// static methods
    const MembersCollection getStaticMethods(StringImpl* identifier, Class klass, bool includeProtocols = true) const {
        MembersCollection methods = this->members(identifier, MemberType::StaticMethod, includeProtocols);

        filterUnavailableMembers<MethodMeta>(methods, klass, true);

        return methods;
    }

    /// instance properties
    const PropertyMeta* instanceProperty(const char* identifier, Class klass, bool includeProtocols = true) const {
        auto propMeta = static_cast<const PropertyMeta*>(this->member(identifier, MemberType::InstanceProperty, includeProtocols));
        return propMeta && propMeta->isAvailableInClass(klass, /*isStatic*/ false) ? propMeta : nullptr;
    }

    const PropertyMeta* instanceProperty(StringImpl* identifier, Class klass, bool includeProtocols = true) const {
        auto propMeta = static_cast<const PropertyMeta*>(this->member(identifier, MemberType::InstanceProperty, includeProtocols));
        return propMeta && propMeta->isAvailableInClass(klass, /*isStatic*/ false) ? propMeta : nullptr;
    }

    /// static properties
    const PropertyMeta* staticProperty(const char* identifier, Class klass, bool includeProtocols = true) const {
        auto propMeta = static_cast<const PropertyMeta*>(this->member(identifier, MemberType::StaticProperty, includeProtocols));
        return propMeta && propMeta->isAvailableInClass(klass, /*isStatic*/ true) ? propMeta : nullptr;
    }

    const PropertyMeta* staticProperty(StringImpl* identifier, Class klass, bool includeProtocols = true) const {
        auto propMeta = static_cast<const PropertyMeta*>(this->member(identifier, MemberType::StaticProperty, includeProtocols));
        return propMeta && propMeta->isAvailableInClass(klass, /*isStatic*/ true) ? propMeta : nullptr;
    }

    /// vectors
    std::vector<const PropertyMeta*> instanceProperties(Class klass) const {
        std::vector<const PropertyMeta*> properties;
        return this->instanceProperties(properties, klass);
    }

    std::vector<const PropertyMeta*> instancePropertiesWithProtocols(Class klass) const {
        std::vector<const PropertyMeta*> properties;
        return this->instancePropertiesWithProtocols(properties, klass);
    }

    std::vector<const PropertyMeta*> instanceProperties(std::vector<const PropertyMeta*>& container, Class klass) const {
        for (Array<PtrTo<PropertyMeta>>::iterator it = this->instanceProps->begin(); it != this->instanceProps->end(); it++) {
            if ((*it)->isAvailableInClass(klass, /*isStatic*/ false)) {
                container.push_back((*it).valuePtr());
            }
        }
        return container;
    }

    std::vector<const PropertyMeta*> instancePropertiesWithProtocols(std::vector<const PropertyMeta*>& container, Class klass) const;

    std::vector<const PropertyMeta*> staticProperties(Class klass) const {
        std::vector<const PropertyMeta*> properties;
        return this->staticProperties(properties, klass);
    }

    std::vector<const PropertyMeta*> staticPropertiesWithProtocols(Class klass) const {
        std::vector<const PropertyMeta*> properties;
        return this->staticPropertiesWithProtocols(properties, klass);
    }

    std::vector<const PropertyMeta*> staticProperties(std::vector<const PropertyMeta*>& container, Class klass) const {
        for (Array<PtrTo<PropertyMeta>>::iterator it = this->staticProps->begin(); it != this->staticProps->end(); it++) {
            if ((*it)->isAvailableInClass(klass, /*isStatic*/ true)) {
                container.push_back((*it).valuePtr());
            }
        }
        return container;
    }

    std::vector<const PropertyMeta*> staticPropertiesWithProtocols(std::vector<const PropertyMeta*>& container, Class klass) const;

    std::vector<const MethodMeta*> initializers(Class klass) const {
        std::vector<const MethodMeta*> initializers;
        return this->initializers(initializers, klass);
    }

    std::vector<const MethodMeta*> initializersWithProtocols(Class klass) const {
        std::vector<const MethodMeta*> initializers;
        return this->initializersWithProtocols(initializers, klass);
    }

    std::vector<const MethodMeta*> initializers(std::vector<const MethodMeta*>& container, Class klass) const;

    std::vector<const MethodMeta*> initializersWithProtocols(std::vector<const MethodMeta*>& container, Class klass) const;
};

struct ProtocolMeta : BaseClassMeta {
};

struct InterfaceMeta : BaseClassMeta {

private:
    String _baseName;

public:
    const char* baseName() const {
        return _baseName.valuePtr();
    }

    const InterfaceMeta* baseMeta() const {
        if (this->baseName() != nullptr) {
            const InterfaceMeta* baseMeta = MetaFile::instance()->globalTable()->findInterfaceMeta(this->baseName());
            return baseMeta;
        }

        return nullptr;
    }
};

#pragma pack(pop)
} // namespace Metadata

#endif /* defined(__NativeScript__Metadata__) */
