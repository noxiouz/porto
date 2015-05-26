#include "libporto.hpp"

using std::string;
using std::vector;

extern "C" {
#include <unistd.h>
}

int TPortoAPI::SendReceive(int fd, rpc::TContainerRequest &req, rpc::TContainerResponse &rsp) {
    google::protobuf::io::FileInputStream pist(fd);
    google::protobuf::io::FileOutputStream post(fd);

    WriteDelimitedTo(req, &post);
    post.Flush();

    if (ReadDelimitedFrom(&pist, &rsp)) {
        LastErrorMsg = rsp.errormsg();
        LastError = (int)rsp.error();
        return LastError;
    } else {
        return -1;
    }
}

TPortoAPI::TPortoAPI(const std::string &path, int retries) : Fd(-1), Retries(retries), RpcSocketPath(path) {
}

TPortoAPI::~TPortoAPI() {
    Cleanup();
}

int TPortoAPI::Rpc(rpc::TContainerRequest &req, rpc::TContainerResponse &rsp) {
    int ret;
    int retries = Retries;
    LastErrorMsg = "";
    LastError = (int)EError::Unknown;

retry:
    if (Fd < 0) {
        TError error = ConnectToRpcServer(RpcSocketPath, Fd);
        if (error) {
            LastErrorMsg = error.GetMsg();
            LastError = INT_MIN;

            if (error.GetErrno() == EACCES || error.GetErrno() == ENOENT)
                goto exit;

            goto exit_or_retry;
        }
    }

    rsp.Clear();
    ret = SendReceive(Fd, req, rsp);
    if (ret < 0) {
        Cleanup();

exit_or_retry:
        if (retries--) {
            usleep(RetryDelayUs);
            goto retry;
        }
    }

exit:
    req.Clear();

    return LastError;
}

int TPortoAPI::Raw(const std::string &message, string &responce) {
    if (!google::protobuf::TextFormat::ParseFromString(message, &Req) ||
        !Req.IsInitialized())
        return -1;

    int ret = Rpc(Req, Rsp);
    if (!ret)
        responce = Rsp.ShortDebugString();

    return ret;
}

int TPortoAPI::Create(const string &name) {
    Req.mutable_create()->set_name(name);

    return Rpc(Req, Rsp);
}

int TPortoAPI::Destroy(const string &name) {
    Req.mutable_destroy()->set_name(name);

    return Rpc(Req, Rsp);
}

int TPortoAPI::List(vector<string> &clist) {
    Req.mutable_list();

    int ret = Rpc(Req, Rsp);
    if (!ret) {
        for (int i = 0; i < Rsp.list().name_size(); i++)
            clist.push_back(Rsp.list().name(i));
    }

    return ret;
}

int TPortoAPI::Plist(vector<TProperty> &plist) {
    Req.mutable_propertylist();

    int ret = Rpc(Req, Rsp);
    if (!ret) {
        auto list = Rsp.propertylist();

        for (int i = 0; i < list.list_size(); i++)
            plist.push_back(TProperty(list.list(i).name(),
                                      list.list(i).desc()));
    }

    return ret;
}

int TPortoAPI::Dlist(vector<TData> &dlist) {
    Req.mutable_datalist();

    int ret = Rpc(Req, Rsp);
    if (!ret) {
        auto list = Rsp.datalist();

        for (int i = 0; i < list.list_size(); i++)
            dlist.push_back(TData(list.list(i).name(),
                                  list.list(i).desc()));
    }

    return ret;
}

int TPortoAPI::Get(const std::vector<std::string> &name,
                   const std::vector<std::string> &variable,
                   std::map<std::string, std::map<std::string, TPortoGetResponse>> &result) {
    auto get = Req.mutable_get();

    for (auto n : name)
        get->add_name(n);
    for (auto v : variable)
        get->add_variable(v);

    int ret = Rpc(Req, Rsp);
    if (!ret) {
         for (int i = 0; i < Rsp.get().list_size(); i++) {
             auto entry = Rsp.get().list(i);
             auto name = entry.name();

             for (int j = 0; j < entry.keyval_size(); j++) {
                 auto keyval = entry.keyval(j);

                 TPortoGetResponse resp;
                 resp.Error = 0;
                 if (keyval.has_error())
                     resp.Error = keyval.error();
                 if (keyval.has_errormsg())
                     resp.ErrorMsg = keyval.errormsg();
                 if (keyval.has_value())
                     resp.Value = keyval.value();

                 result[name][keyval.variable()] = resp;
             }
         }
    }

    return ret;
}

int TPortoAPI::GetProperty(const string &name, const string &property, string &value) {
    Req.mutable_getproperty()->set_name(name);
    Req.mutable_getproperty()->set_property(property);

    int ret = Rpc(Req, Rsp);
    if (!ret)
        value.assign(Rsp.getproperty().value());

    return ret;
}

int TPortoAPI::SetProperty(const string &name, const string &property, string value) {
    Req.mutable_setproperty()->set_name(name);
    Req.mutable_setproperty()->set_property(property);
    Req.mutable_setproperty()->set_value(value);

    return Rpc(Req, Rsp);
}

int TPortoAPI::GetData(const string &name, const string &data, string &value) {
    Req.mutable_getdata()->set_name(name);
    Req.mutable_getdata()->set_data(data);

    int ret = Rpc(Req, Rsp);
    if (!ret)
        value.assign(Rsp.getdata().value());

    return ret;
}

int TPortoAPI::GetVersion(std::string &tag, std::string &revision) {
    Req.mutable_version();

    int ret = Rpc(Req, Rsp);
    if (!ret) {
        tag = Rsp.version().tag();
        revision = Rsp.version().revision();
    }

    return ret;
}

int TPortoAPI::Start(const string &name) {
    Req.mutable_start()->set_name(name);

    return Rpc(Req, Rsp);
}

int TPortoAPI::Stop(const string &name) {
    Req.mutable_stop()->set_name(name);

    return Rpc(Req, Rsp);
}

int TPortoAPI::Kill(const std::string &name, int sig) {
    Req.mutable_kill()->set_name(name);
    Req.mutable_kill()->set_sig(sig);

    return Rpc(Req, Rsp);
}

int TPortoAPI::Pause(const string &name) {
    Req.mutable_pause()->set_name(name);

    return Rpc(Req, Rsp);
}

int TPortoAPI::Resume(const string &name) {
    Req.mutable_resume()->set_name(name);

    return Rpc(Req, Rsp);
}

int TPortoAPI::Wait(const std::vector<std::string> &containers, std::string &name) {
    for (auto &c : containers)
        Req.mutable_wait()->add_name(c);

    int ret = Rpc(Req, Rsp);
    name.assign(Rsp.wait().name());
    return ret;
}

void TPortoAPI::GetLastError(int &error, std::string &msg) const {
    error = LastError;
    msg = LastErrorMsg;
}

void TPortoAPI::Cleanup() {
    close(Fd);
    Fd = -1;
}

int TPortoAPI::CreateVolume(const std::string &path, const std::string &source,
                            const std::string &quota, const std::string &flags) {
    Req.mutable_createvolume()->set_path(path);
    Req.mutable_createvolume()->set_source(source);
    Req.mutable_createvolume()->set_quota(quota);
    Req.mutable_createvolume()->set_flags(flags);

    return Rpc(Req, Rsp);
}

int TPortoAPI::DestroyVolume(const std::string &path) {
    Req.mutable_destroyvolume()->set_path(path);

    return Rpc(Req, Rsp);
}

int TPortoAPI::ListVolumes(std::vector<TVolumeDescription> &vlist) {
    Req.mutable_listvolumes();

    int ret = Rpc(Req, Rsp);
    if (!ret) {
        auto list = Rsp.volumelist();

        for (int i = 0; i < list.list_size(); i++)
            vlist.push_back(TVolumeDescription(list.list(i).path(),
                                               list.list(i).source(),
                                               list.list(i).quota(),
                                               list.list(i).flags(),
                                               list.list(i).used(),
                                               list.list(i).avail()));
    }

    return ret;
}
