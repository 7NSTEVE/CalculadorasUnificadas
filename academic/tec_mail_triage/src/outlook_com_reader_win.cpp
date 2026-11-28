#include "academic/tec_mail_triage/triage_engine.hpp"

#include <windows.h>
#include <oleauto.h>

#include <string>

namespace academic::tec_mail_triage {

namespace {

std::string WideToUtf8(const wchar_t* w) {
    if (!w || !w[0]) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string s(static_cast<size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n - 1, nullptr, nullptr);
    return s;
}

HRESULT ComInvoke(IDispatch* disp, WORD flags, const wchar_t* name, VARIANT* out_res,
                  VARIANT* args = nullptr, UINT arg_count = 0)
{
    if (!disp) return E_POINTER;
    DISPID dispid = 0;
    LPOLESTR nm = const_cast<LPOLESTR>(name);
    HRESULT hr = disp->GetIDsOfNames(IID_NULL, &nm, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) return hr;
    DISPPARAMS dp{};
    dp.rgvarg = args;
    dp.cArgs = arg_count;
    return disp->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, flags, &dp, out_res, nullptr, nullptr);
}

std::string GetStringProp(IDispatch* disp, const wchar_t* prop) {
    VARIANT v;
    VariantInit(&v);
    std::string out;
    if (SUCCEEDED(ComInvoke(disp, DISPATCH_PROPERTYGET, prop, &v)) && v.vt == VT_BSTR && v.bstrVal) {
        out = WideToUtf8(v.bstrVal);
    }
    VariantClear(&v);
    return out;
}

bool GetBoolProp(IDispatch* disp, const wchar_t* prop) {
    VARIANT v;
    VariantInit(&v);
    bool out = false;
    if (SUCCEEDED(ComInvoke(disp, DISPATCH_PROPERTYGET, prop, &v)) && v.vt == VT_BOOL) {
        out = (v.boolVal != VARIANT_FALSE);
    }
    VariantClear(&v);
    return out;
}

int GetIntProp(IDispatch* disp, const wchar_t* prop) {
    VARIANT v;
    VariantInit(&v);
    int out = 0;
    if (SUCCEEDED(ComInvoke(disp, DISPATCH_PROPERTYGET, prop, &v))) {
        if (v.vt == VT_I4) out = v.lVal;
        else if (v.vt == VT_I2) out = v.iVal;
    }
    VariantClear(&v);
    return out;
}

void ReadFolderRecursive(IDispatch* folder, const std::string& path, bool in_spam,
                         int max_messages, int max_body,
                         std::vector<MailMessage>& out_msgs,
                         std::vector<std::string>& out_errors)
{
    if (!folder || static_cast<int>(out_msgs.size()) >= max_messages) return;
    VARIANT vItems;
    VariantInit(&vItems);
    if (FAILED(ComInvoke(folder, DISPATCH_PROPERTYGET, L"Items", &vItems)) || vItems.vt != VT_DISPATCH || !vItems.pdispVal) {
        out_errors.push_back(path + ": no se pudo acceder a Items.");
        VariantClear(&vItems);
        return;
    }
    IDispatch* items = vItems.pdispVal;
    const int count = GetIntProp(items, L"Count");
    for (int i = count; i >= 1 && static_cast<int>(out_msgs.size()) < max_messages; --i) {
        VARIANT arg;
        VariantInit(&arg);
        arg.vt = VT_I4;
        arg.lVal = i;
        VARIANT vItem;
        VariantInit(&vItem);
        if (SUCCEEDED(ComInvoke(items, DISPATCH_METHOD | DISPATCH_PROPERTYGET, L"Item", &vItem, &arg, 1)) &&
            vItem.vt == VT_DISPATCH && vItem.pdispVal) {
            const int cls = GetIntProp(vItem.pdispVal, L"Class");
            if (cls == 43 || cls == 53) {
                MailMessage m{};
                m.id = GetStringProp(vItem.pdispVal, L"EntryID");
                m.subject = GetStringProp(vItem.pdispVal, L"Subject");
                m.body = GetStringProp(vItem.pdispVal, L"Body");
                if (static_cast<int>(m.body.size()) > max_body) m.body.resize(static_cast<size_t>(max_body));
                m.sender_name = GetStringProp(vItem.pdispVal, L"SenderName");
                m.sender_smtp = GetStringProp(vItem.pdispVal, L"SenderEmailAddress");
                m.unread = GetBoolProp(vItem.pdispVal, L"UnRead");
                m.is_meeting = (cls == 53);
                m.in_spam = in_spam;
                m.folder = path;
                out_msgs.push_back(std::move(m));
            }
        }
        VariantClear(&vItem);
    }
    VariantClear(&vItems);
}

} // namespace

OutlookReadResult OutlookComReader::ReadMailboxes(int max_messages, int max_body_chars) {
    OutlookReadResult res{};
    const HRESULT init_hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool did_init = SUCCEEDED(init_hr);

    CLSID clsid{};
    if (FAILED(CLSIDFromProgID(L"Outlook.Application", &clsid))) {
        res.error_code = "NO_COM";
        res.error_message = "Outlook clasico no esta instalado o no expone COM.";
        res.fix_hint = "Instale Outlook clasico (Office) y configure la cuenta @estudiantec.cr.";
        if (did_init) CoUninitialize();
        return res;
    }

    IDispatch* app = nullptr;
    if (FAILED(CoCreateInstance(clsid, nullptr, CLSCTX_LOCAL_SERVER, IID_IDispatch, reinterpret_cast<void**>(&app))) || !app) {
        res.error_code = "NO_COM";
        res.error_message = "No se pudo instanciar Outlook.Application via COM.";
        res.fix_hint = "Verifique que Outlook clasico este instalado y sin bloqueos.";
        if (did_init) CoUninitialize();
        return res;
    }

    VARIANT arg_mapi;
    VariantInit(&arg_mapi);
    arg_mapi.vt = VT_BSTR;
    arg_mapi.bstrVal = SysAllocString(L"MAPI");
    VARIANT vNs;
    VariantInit(&vNs);
    if (FAILED(ComInvoke(app, DISPATCH_METHOD, L"GetNamespace", &vNs, &arg_mapi, 1)) ||
        vNs.vt != VT_DISPATCH || !vNs.pdispVal) {
        VariantClear(&arg_mapi);
        app->Release();
        res.error_code = "NO_MAPI";
        res.error_message = "No se pudo abrir el espacio de nombres MAPI de Outlook.";
        if (did_init) CoUninitialize();
        return res;
    }
    VariantClear(&arg_mapi);

    auto read_default = [&](int folder_id, const std::string& label, bool spam) -> bool {
        VARIANT fid;
        VariantInit(&fid);
        fid.vt = VT_I4;
        fid.lVal = folder_id;
        VARIANT vF;
        VariantInit(&vF);
        const bool ok = SUCCEEDED(ComInvoke(vNs.pdispVal, DISPATCH_METHOD, L"GetDefaultFolder", &vF, &fid, 1)) &&
                        vF.vt == VT_DISPATCH && vF.pdispVal;
        if (ok) {
            ReadFolderRecursive(vF.pdispVal, label, spam, max_messages, max_body_chars, res.messages, res.folder_errors);
        }
        VariantClear(&vF);
        return ok;
    };

    read_default(6, "Inbox", false);
    res.spam_read = read_default(23, "Correo no deseado", true);
    res.ok = true;

    VariantClear(&vNs);
    app->Release();
    if (did_init) CoUninitialize();
    return res;
}

} // namespace academic::tec_mail_triage
