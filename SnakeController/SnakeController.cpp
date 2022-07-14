#include "SnakeController.hpp"

#include <algorithm>
#include <sstream>

#include "EventT.hpp"
#include "IPort.hpp"

namespace Snake
{
ConfigurationError::ConfigurationError()
    : std::logic_error("Bad configuration of Snake::Controller.")
{}

UnexpectedEventException::UnexpectedEventException()
    : std::runtime_error("Unexpected event received!")
{}

Controller::Controller(IPort& p_displayPort, IPort& p_foodPort, IPort& p_scorePort, std::string const& p_config)
    : m_displayPort(p_displayPort),
      m_foodPort(p_foodPort),
      m_scorePort(p_scorePort)
{
    std::istringstream istr(p_config);
    char w, f, s, d;

    int width, height, length;
    int foodX, foodY;
    istr >> w >> width >> height >> f >> foodX >> foodY >> s;

    if (w == 'W' and f == 'F' and s == 'S') {
        m_mapDimension = std::make_pair(width, height);
        m_foodPosition = std::make_pair(foodX, foodY);

        istr >> d;
        switch (d) {
            case 'U':
                m_currentDirection = Direction_UP;
                break;
            case 'D':
                m_currentDirection = Direction_DOWN;
                break;
            case 'L':
                m_currentDirection = Direction_LEFT;
                break;
            case 'R':
                m_currentDirection = Direction_RIGHT;
                break;
            default:
                throw ConfigurationError();
        }
        istr >> length;

        while (length) {
            Segment seg;
            istr >> seg.x >> seg.y;
            seg.ttl = length--;

            m_segments.push_back(seg);
        }
    } else {
        throw ConfigurationError();
    }
}

void Controller::setNewHeadPosition(Segment &newHead) {
    Segment const& currentHead = m_segments.front();
    newHead.x = currentHead.x + ((m_currentDirection & Direction_LEFT) ? (m_currentDirection & Direction_DOWN) ? 1 : -1 : 0);
    newHead.y = currentHead.y + (not (m_currentDirection & Direction_LEFT) ? (m_currentDirection & Direction_DOWN) ? 1 : -1 : 0);
    newHead.ttl = currentHead.ttl;
}

bool Controller::checkIfLost(Segment& newHead) {

    for (auto segment : m_segments) {
        if (segment.x == newHead.x and segment.y == newHead.y) {
            m_scorePort.send(std::make_unique<EventT<LooseInd>>());
            return true;
        }
    }
    return false;
}

bool Controller::checkOutOfBounds(Segment &newHead) {
    if(newHead.x < 0 or newHead.y < 0 or newHead.x >= m_mapDimension.first or newHead.y >= m_mapDimension.second)
    {
//        m_scorePort.send(std::make_unique<EventT<LooseInd>>());
        return true;
    }
    return false;
}

void Controller::deleteOldSnake() {
    for (auto &segment : m_segments) {
        if (not --segment.ttl) {
            DisplayInd l_evt;
            l_evt.x = segment.x;
            l_evt.y = segment.y;
            l_evt.value = Cell_FREE;

            m_displayPort.send(std::make_unique<EventT<DisplayInd>>(l_evt));
        }
    }
}

void Controller::moveSnake(Segment& newHead){
        m_segments.push_front(newHead);
        DisplayInd placeNewHead;
        placeNewHead.x = newHead.x;
        placeNewHead.y = newHead.y;
        placeNewHead.value = Cell_SNAKE;

        m_displayPort.send(std::make_unique<EventT<DisplayInd>>(placeNewHead));

        m_segments.erase(
                std::remove_if(
                        m_segments.begin(),
                        m_segments.end(),
                        [](auto const& segment){ return not (segment.ttl > 0); }),
                m_segments.end());
};
template<typename T>
bool Controller::colidedWithFood(const T& receivedFood) {

    for (auto const& segment : m_segments)
        if (segment.x == receivedFood.x and segment.y == receivedFood.y)
            return true;
    return false;
}

template<typename T>
T Controller::castToTEvent(std::unique_ptr<Event>& e) {
    return *dynamic_cast<EventT<T> const&>(*e);
}
void Controller::receive(std::unique_ptr<Event> e)
{
    try {
        *dynamic_cast<EventT<TimeoutInd> const&>(*e);

        Segment newHead{};
        setNewHeadPosition(newHead);

        bool lost = false;

        lost = checkIfLost(newHead);

        if (not lost) {
            if (std::make_pair(newHead.x, newHead.y) == m_foodPosition) {
                m_scorePort.send(std::make_unique<EventT<ScoreInd>>());
                m_foodPort.send(std::make_unique<EventT<FoodReq>>());
            } else if (checkOutOfBounds(newHead)) {
                m_scorePort.send(std::make_unique<EventT<LooseInd>>());
                lost = true;
            } else {
                deleteOldSnake();
            }
        }

        if (not lost)
           moveSnake(newHead);

    } catch (std::bad_cast&) {
        try {

            if ((m_currentDirection & Direction_LEFT) != (dynamic_cast<EventT<DirectionInd> const&>(*e)->direction & Direction_LEFT)) {
                    m_currentDirection = castToTEvent<DirectionInd>(e).direction;
            }
        } catch (std::bad_cast&) {
            try {
                if (colidedWithFood(castToTEvent<FoodInd>(e))) {
                    m_foodPort.send(std::make_unique<EventT<FoodReq>>());
                } else {

                    m_displayPort.send(std::make_unique<EventT<DisplayInd>>(DisplayInd{m_foodPosition.first,m_foodPosition.second, Cell_FREE}));
                    m_displayPort.send(std::make_unique<EventT<DisplayInd>>(DisplayInd{castToTEvent<FoodInd>(e).x,castToTEvent<FoodInd>(e).y,Cell_FOOD}));
                }
                m_foodPosition = std::make_pair(castToTEvent<FoodInd>(e).x, castToTEvent<FoodInd>(e).y);

            } catch (std::bad_cast&) {
                try {
                    if (colidedWithFood(castToTEvent<FoodResp>(e))) {
                        m_foodPort.send(std::make_unique<EventT<FoodReq>>());
                    } else {

                        m_displayPort.send(std::make_unique<EventT<DisplayInd>>(DisplayInd{castToTEvent<FoodResp>(e).x,castToTEvent<FoodResp>(e).y, Cell_FOOD}));
                    }
                    m_foodPosition = std::make_pair(castToTEvent<FoodResp>(e).x, castToTEvent<FoodResp>(e).y);
                } catch (std::bad_cast&) {
                    throw UnexpectedEventException();
                }
            }
        }
    }
}

} // namespace Snake
