#pragma once
#include <cstdint>
namespace NL {
template<class Reference,class Handler> Reference* resolveReference(Handler* handler,std::uint32_t id) {
    auto* form=handler->LookupForm(id,"Skyrim.esm");return form ? form->template As<Reference>() : nullptr;
}
}
