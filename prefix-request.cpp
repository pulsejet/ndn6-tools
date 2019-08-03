#include <ndn-cxx/face.hpp>
#include <ndn-cxx/lp/tags.hpp>
#include <ndn-cxx/mgmt/nfd/controller.hpp>
#include <ndn-cxx/util/sha256.hpp>

#include <cstdio>
#include <iostream>
#include <time.h>

namespace ndn {
namespace prefix_request {

static const std::string TAB = "\t";
static const int ORIGIN_PREFIX_REQUEST = 19438;

class PrefixRequest : noncopyable
{
public:
  explicit
  PrefixRequest(const std::string& secret)
    : m_controller(m_face, m_keyChain)
    , m_secret(secret)
  {
  }

  void
  run()
  {
    nfd::ControlParameters p1;
    p1.setFlagBit(nfd::BIT_LOCAL_FIELDS_ENABLED, true);
    m_controller.start<nfd::FaceUpdateCommand>(p1,
      nullptr, std::bind(&std::exit, 1));

    m_face.setInterestFilter("/localhop/prefix-request",
      bind(&PrefixRequest::processCommand, this, _2), bind(&std::exit, 1));

    m_face.processEvents();
  }

private:
  void
  processCommand(const Interest& interest)
  {
    // /localhop/prefix-request/<prefix-uri>/<answer>/<random>
    // <answer> == SHA256(<secret> <prefix-uri>) in upper case
    if (interest.getName().size() != 5) {
      return; // bad command
    }

    // parse prefix
    const name::Component& prefixUriArg = interest.getName().at(2);
    std::string prefixUri(reinterpret_cast<const char*>(prefixUriArg.value()),
                          prefixUriArg.value_size());

    Name prefix;
    try {
      prefix = Name(prefixUri);
    }
    catch (const Name::Error&) {
      return; // cannot decode Name
    }
    if (prefix.toUri() != prefixUri) {
      return; // URI not canonical
    }

    auto incomingFaceIdTag = interest.getTag<ndn::lp::IncomingFaceIdTag>();
    if (incomingFaceIdTag == nullptr) {
      return;
    }

    nfd::ControlParameters p;
    p.setName(prefix);
    p.setFaceId(*incomingFaceIdTag);
    p.setOrigin(static_cast<nfd::RouteOrigin>(ORIGIN_PREFIX_REQUEST));
    p.setCost(800);

    // verify answer
    if (!verifyAnswer(interest)) {
      std::cout << ::time(nullptr) << TAB
                << 8401 << TAB
                << p.getFaceId() << TAB
                << p.getName() << std::endl;
      return;
    }

    m_controller.start<nfd::RibRegisterCommand>(p,
      bind(&PrefixRequest::onRegisterSucceed, this, _1, interest),
      bind(&PrefixRequest::onRegisterFail, this, p, _1));
  }

  bool
  verifyAnswer(const Interest& interest)
  {
    const name::Component& prefixUriArg = interest.getName().at(2);

    const name::Component& answerArg = interest.getName().at(3);
    std::string answer(reinterpret_cast<const char*>(answerArg.value()), answerArg.value_size());

    util::Sha256 digest;
    digest << m_secret;
    digest.update(prefixUriArg.value(), prefixUriArg.value_size());
    std::string answer2 = digest.toString();

    if (answer.size() != answer2.size()) {
      return false;
    }

    int diff = 0;
    for (size_t pos = 0; pos < answer.size(); ++pos) {
      diff |= answer[pos] ^ answer2[pos];
    }
    return diff == 0;
  }

  void
  onRegisterSucceed(const nfd::ControlParameters& p, const Interest& interest)
  {
    shared_ptr<Data> data = make_shared<Data>(interest.getName());
    data->setContent(p.getName().wireEncode());
    m_keyChain.sign(*data);
    m_face.put(*data);

    std::cout << ::time(nullptr) << TAB
              << 0 << TAB
              << p.getFaceId() << TAB
              << p.getName() << std::endl;
  }

  void
  onRegisterFail(const nfd::ControlParameters& p, const nfd::ControlResponse& resp)
  {
    std::cout << ::time(nullptr) << TAB
              << resp.getCode() << TAB
              << p.getFaceId() << TAB
              << p.getName() << std::endl;
  }

private:
  Face m_face;
  KeyChain m_keyChain;
  nfd::Controller m_controller;
  std::string m_secret;
};

int
main(int argc, char** argv)
{
  if (argc != 2) {
    std::cerr << "prefix-request secret" << std::endl;
    return -1;
  }

  PrefixRequest app(argv[1]);
  app.run();

  return 0;
}

} // namespace prefix_request
} // namespace ndn

int
main(int argc, char** argv)
{
  return ndn::prefix_request::main(argc, argv);
}
