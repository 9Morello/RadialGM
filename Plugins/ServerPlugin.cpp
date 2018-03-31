#include "ServerPlugin.h"

#if _WIN32_WINNT < 0x0600
#define _WIN32_WINNT 0x0600
#endif
#include "server.grpc.pb.h"

#include <QDebug>
#include <QFileDialog>
#include <QTemporaryFile>

#include <grpc++/channel.h>
#include <grpc++/client_context.h>
#include <grpc++/create_channel.h>
#include <grpc/grpc.h>

using namespace grpc;
using namespace buffers;

class CompilerClient {
 public:
  explicit CompilerClient(std::shared_ptr<Channel> channel) : stub(Compiler::NewStub(channel)) {}

  void CompileBuffer(Game* game, CompileMode mode, std::string name) {
    ClientContext context;
    CompileRequest request;

    request.mutable_game()->CopyFrom(*game);
    request.set_name(name);
    request.set_mode(mode);

    std::unique_ptr<ClientReader<CompileReply> > reader(stub->CompileBuffer(&context, request));
    CompileReply reply;
    while (reader->Read(&reply)) {
      qDebug() << reply.message().c_str() << reply.progress();
    }
    Status status = reader->Finish();
  }

  void CompileBuffer(Game* game, CompileMode mode) {
    QTemporaryFile t("enigma");
    if (!t.open()) return;
    CompileBuffer(game, mode, t.fileName().toStdString());
  }

 private:
  std::unique_ptr<Compiler::Stub> stub;
};

ServerPlugin::ServerPlugin(MainWindow& mainWindow) : RGMPlugin(mainWindow) {
  process = new QProcess(this);

  connect(process, &QProcess::readyReadStandardOutput, this, &ServerPlugin::HandleOutput);
  connect(process, &QProcess::readyReadStandardError, this, &ServerPlugin::HandleError);
  process->setWorkingDirectory("../RadialGM/Submodules/enigma-dev");

  QString program = "emake";
  QStringList arguments;
  arguments << "--server"
            << "--quiet";
  process->start(program, arguments);

  ChannelArguments channelArguments;
  channelArguments.SetMaxSendMessageSize(-1);
  channelArguments.SetMaxReceiveMessageSize(-1);
  auto channel = CreateCustomChannel("localhost:37818", InsecureChannelCredentials(), channelArguments);
  compilerClient = new CompilerClient(channel);
}

ServerPlugin::~ServerPlugin() {
  process->terminate();
  process->waitForFinished();
}

void ServerPlugin::Run() { compilerClient->CompileBuffer(mainWindow.Game(), CompileMode::RUN); };

void ServerPlugin::Debug() { compilerClient->CompileBuffer(mainWindow.Game(), CompileMode::DEBUG); };

void ServerPlugin::CreateExecutable() {
  const QString& fileName =
      QFileDialog::getSaveFileName(&mainWindow, tr("Create Executable"), "", tr("Executable (*.exe);;All Files (*)"));
  if (!fileName.isEmpty())
    compilerClient->CompileBuffer(mainWindow.Game(), CompileMode::COMPILE, fileName.toStdString());
};

void ServerPlugin::HandleOutput() { emit OutputRead(process->readAllStandardOutput()); }

void ServerPlugin::HandleError() { emit ErrorRead(process->readAllStandardError()); }