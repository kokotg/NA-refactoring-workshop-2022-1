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

void Controller::receive(std::unique_ptr<Event> e)
{
    try {
        auto const& timerEvent = *dynamic_cast<EventT<TimeoutInd> const&>(*e);

        changeSnakePosition(m_segments.front());

    } catch (std::bad_cast&) {
        try {
            auto direction = dynamic_cast<EventT<DirectionInd> const&>(*e)->direction;

            if ((m_currentDirection & 0b01) != (direction & 0b01)) {
                m_currentDirection = direction;
            }
        } catch (std::bad_cast&) {
            try {
                auto receivedFood = *dynamic_cast<EventT<FoodInd> const&>(*e);

                updateFood(receivedFood);

                m_foodPosition = std::make_pair(receivedFood.x, receivedFood.y);

            } catch (std::bad_cast&) {
                try {
                    auto requestedFood = *dynamic_cast<EventT<FoodResp> const&>(*e);

                    respawnFood(requestedFood);

                } catch (std::bad_cast&) {
                    throw UnexpectedEventException();
                }
            }
        }
    }
}
bool Controller::collidedWithSnake(int x, int y) {
    for (auto const& segment : m_segments) {
        if (segment.x == x and segment.y == y) {
            return true;
        }
    }
    return false;
}

void Controller::changeCellState(int x, int y, Cell cell) {
    DisplayInd newState;
    newState.x = x;
    newState.y = y;
    newState.value = cell;
    m_displayPort.send(std::make_unique<EventT<DisplayInd>>(newState));
}

bool Controller::collidedWithMap(int x, int y) {
    return  x < 0 or y < 0 or x >= m_mapDimension.first or y >= m_mapDimension.second;
}

void Controller::changeSnakePosition(const Segment &currentHead) {

    Segment newHead;
    newHead.x = currentHead.x + ((m_currentDirection & 0b01) ? (m_currentDirection & 0b10) ? 1 : -1 : 0);
    newHead.y = currentHead.y + (not (m_currentDirection & 0b01) ? (m_currentDirection & 0b10) ? 1 : -1 : 0);
    newHead.ttl = currentHead.ttl;

    bool lost = collidedWithSnake(newHead.x, newHead.y);

    if (lost) {
        m_scorePort.send(std::make_unique<EventT<LooseInd>>());
    } else {
        if (std::make_pair(newHead.x, newHead.y) == m_foodPosition) {
            m_scorePort.send(std::make_unique<EventT<ScoreInd>>());
            m_foodPort.send(std::make_unique<EventT<FoodReq>>());
        } else if (collidedWithMap(newHead.x, newHead.y)) {
            m_scorePort.send(std::make_unique<EventT<LooseInd>>());
            lost = true;
        } else {
            for (auto &segment : m_segments) {
                if (not --segment.ttl) {
                    changeCellState(segment.x, segment.y, Cell_FREE);
                }
            }
        }
    }

    if (not lost) {
        m_segments.push_front(newHead);

        changeCellState(newHead.x, newHead.y, Cell_SNAKE);

        m_segments.erase(
            std::remove_if(
                m_segments.begin(),
                m_segments.end(),
                [](auto const& segment){ return not (segment.ttl > 0); }),
            m_segments.end());
    }
}

void Controller::respawnFood(const FoodResp &request) {

    if (collidedWithSnake(request.x, request.y)) {
        m_foodPort.send(std::make_unique<EventT<FoodReq>>());
    } else {
        changeCellState(request.x, request.y, Cell_FOOD);
    }

    m_foodPosition = std::make_pair(request.x, request.y);
}

void Controller::updateFood(const FoodInd &request) {

    if (collidedWithSnake(request.x, request.y)) {
        m_foodPort.send(std::make_unique<EventT<FoodReq>>());
    } else {
        changeCellState(m_foodPosition.first, m_foodPosition.second, Cell_FREE);
        changeCellState(request.x, request.y, Cell_FOOD);
    }

    m_foodPosition = std::make_pair(request.x, request.y);
}

} // namespace Snake
