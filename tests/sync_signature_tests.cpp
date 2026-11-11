#include "sync_signature.h"
#include <cassert>
#include <string>

int main() {
    const std::string version="b80dd203afaaaa6d8a58fb9ac4e24b08b33fbab5";
    const std::string hash="0b0e5ac7d8f5e897a774f5b55d1f2228047659a09171d7af756aa36bf9b86ff9";
    const std::string signature="fzn8ymVuyCx9oSRvs1uw8FkzxcAByimNBKSZfTyqS/amfvFsyDbEZJkejCCvHzv1HQUEQJeolY907T8gW7sJWoYSJVVDHgDW4KFNW4NG6DvPzYC5NFlwM2orfaWMyZ0estQSv3U8Xdyj1rXd//d3MQEhG3pLagjKKgPyeSvO3p1MlzuUaF9zYHAoF9NrS2uG4B2wN8Eu3zCg9rmuSvRrDFEH8BsStPisQbRTuEmWA59Fq1r+gXV3EZsnz0YqM3Ljej0CCT3I1Y3+fd1hz/aM/smugk3Pca++Xe7iGge304zCdHbympItdV+szv3BHpdqd+916ug3gDLlCUerq8zR3w==";
    assert(josts::verify_sync_signature(version,hash,signature));
    assert(!josts::verify_sync_signature(version+"x",hash,signature));
    assert(!josts::verify_sync_signature(version,"1"+hash.substr(1),signature));
    std::string altered=signature; altered[12]=altered[12]=='A'?'B':'A';
    assert(!josts::verify_sync_signature(version,hash,altered));
    return 0;
}
