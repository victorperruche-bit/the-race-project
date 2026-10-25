#include "CapteurUltrason.h"
#include "Config.h"

CapteurUltrason::CapteurUltrason(const std::string& etiquette)
    : etiquette_(etiquette)
{
}

void CapteurUltrason::setDistance(float distanceCm)
{
    distance_ = distanceCm;
}

float CapteurUltrason::mesurer() const
{
    // Mesure invalide si hors plage crédible
    if (distance_ < config::DISTANCE_MIN_CM || distance_ > config::DISTANCE_MAX_CM) {
        return -1.0f;
    }
    return distance_;
}
