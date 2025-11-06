#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>
#include <cassert>

namespace bj {

// ---------- Cards ----------
enum class Suit : uint8_t { Clubs, Diamonds, Hearts, Spades };
enum class Rank : uint8_t { Two=2, Three, Four, Five, Six, Seven, Eight, Nine, Ten, Jack, Queen, King, Ace };

struct Card {
  Rank rank;
  Suit suit;
  std::string short_str() const {
    static constexpr std::array<std::string_view, 13> r = {"2","3","4","5","6","7","8","9","T","J","Q","K","A"};
    static constexpr std::array<std::string_view, 4> s = {"C","D","H","S"};
    return std::string(r[static_cast<int>(rank)]) + std::string(s[static_cast<int>(suit)]);
  }
};

inline int card_value(Rank r) {
  switch (r) {
    case Rank::Two: case Rank::Three: case Rank::Four: case Rank::Five:
    case Rank::Six: case Rank::Seven: case Rank::Eight: case Rank::Nine:
      return static_cast<int>(r);
    case Rank::Ten: case Rank::Jack: case Rank::Queen: case Rank::King:
      return 10;
    case Rank::Ace:
      return 11; // count as 11 first; adjust down later
  }
  return 0;
}

// ---------- Rules ----------
struct Rules {
  int num_decks = 6;
  bool dealer_hits_soft17 = true;   // H17 (true) vs S17 (false)
  bool double_allowed = true;
  bool double_after_split = true;
  bool surrender = false;           // late surrender
  bool peek_for_blackjack = true;   // peek on Ten/Ace upcard
  int blackjack_pays_num = 3;       // 3:2 typical. For 6:5 use 6 and 5.
  int blackjack_pays_den = 2;
};

// ---------- Shoe (multi-deck) ----------
class Shoe {
 public:
  explicit Shoe(int decks = 6, uint64_t seed = std::random_device{}())
  : rng_(seed) { reset(decks); }

  void reset(int decks) {
    cards_.clear(); cards_.reserve(decks * 52);
    for (int d = 0; d < decks; ++d) {
      for (int s = 0; s < 4; ++s) {
        for (int r = 2; r <= 14; ++r) {
          cards_.push_back(Card{static_cast<Rank>(r), static_cast<Suit>(s)});
        }
      }
    }
    shuffle();
    next_ = 0;
  }

  void shuffle() {
    std::shuffle(cards_.begin(), cards_.end(), rng_);
  }

  Card draw() {
    if (next_ >= cards_.size()) { shuffle(); next_ = 0; }
    return cards_[next_++];
  }

  size_t remaining() const { return cards_.size() - next_; }

 private:
  std::vector<Card> cards_;
  size_t next_ = 0;
  std::mt19937_64 rng_;
};

// ---------- Hand ----------
struct Hand {
  std::vector<Card> cards;
  bool doubled = false;  // track double-down
  bool surrendered = false;

  void add(Card c) { cards.push_back(c); }

  int hard_total() const {
    int total = 0;
    for (auto &c : cards) total += card_value(c.rank);
    int aces = 0;
    for (auto &c : cards) if (c.rank == Rank::Ace) ++aces;

    // Reduce Ace values (11 -> 1) as needed
    while (total > 21 && aces > 0) { total -= 10; --aces; }
    return total;
  }

  bool is_soft() const {
    int total = 0; int aces = 0;
    for (auto &c : cards) { total += card_value(c.rank); if (c.rank == Rank::Ace) ++aces; }
    while (aces > 0 && total > 21) { total -= 10; --aces; }
    // soft if there exists an Ace still counted as 11
    return std::any_of(cards.begin(), cards.end(), [&](const Card& c){
      if (c.rank != Rank::Ace) return false;
      // recompute treating this ace as 11 if possible
      int base = 0; int a = 0;
      for (auto &x : cards) { base += card_value(x.rank); if (x.rank==Rank::Ace) ++a; }
      while (a > 0 && base > 21) { base -= 10; --a; }
      return base <= 21 && std::count_if(cards.begin(), cards.end(), [](const Card& t){return t.rank==Rank::Ace;})>0
             && base != hard_total(); // crude but fine
    });
  }

  bool is_blackjack() const {
    return cards.size() == 2 && hard_total() == 21;
  }
  bool is_bust() const { return hard_total() > 21; }
};

// ---------- Decisions & Strategy ----------
enum class Decision { Hit, Stand, Double, Surrender };

struct Situation {
  const Hand& player;
  const Hand& dealer; // only dealer upcard is relevant for strategy, but we include full for settlement.
  const Rules& rules;
  bool can_double = true;
};

struct Strategy {
  virtual ~Strategy() = default;
  virtual Decision decide(const Situation& s) = 0;
};

// A very naive baseline strategy (replace with a real basic strategy later)
struct AlwaysHitUnder17 : Strategy {
  Decision decide(const Situation& s) override {
    if (s.rules.double_allowed && s.can_double && s.player.cards.size()==2 && s.player.hard_total() >= 9 && s.player.hard_total() <= 11)
      return Decision::Double;
    if (s.player.hard_total() < 17) return Decision::Hit;
    return Decision::Stand;
  }
};

// ---------- Round / Outcomes ----------
enum class Outcome { PlayerBJ, DealerBJ, PlayerBust, DealerBust, PlayerWin, DealerWin, Push, PlayerSurrender };

struct RoundResult {
  Outcome outcome;
  int player_total;
  int dealer_total;
  int64_t payout_cents;  // settlement for a 1.00 unit bet would be 100
};

class Round {
 public:
  Round(const Rules& rules, Shoe& shoe, Strategy& strat, int64_t bet_cents = 100)
  : rules_(rules), shoe_(shoe), strat_(strat), bet_(bet_cents) {}

  RoundResult play() {
    player_ = Hand{}; dealer_ = Hand{};
    // deal initial
    player_.add(shoe_.draw());
    dealer_.add(shoe_.draw());
    player_.add(shoe_.draw());
    dealer_.add(shoe_.draw());

    // Peek for dealer blackjack (if upcard is Ace/Ten and rules allow)
    if (rules_.peek_for_blackjack) {
      if (dealer_.is_blackjack()) {
        if (player_.is_blackjack()) return settle(Outcome::Push);
        return settle(Outcome::DealerBJ);
      }
    }

    // Player blackjack immediate
    if (player_.is_blackjack()) return settle(Outcome::PlayerBJ);

    // PLAYER TURN
    bool can_double = rules_.double_allowed;
    while (true) {
      // Offer surrender on first decision if enabled
      if (rules_.surrender && player_.cards.size()==2) {
        Situation s{player_, dealer_, rules_, can_double};
        if (strat_.decide(s) == Decision::Surrender) {
          player_.surrendered = true;
          return settle(Outcome::PlayerSurrender);
        }
      }

      Situation s{player_, dealer_, rules_, can_double};
      Decision d = strat_.decide(s);

      if (d == Decision::Double && can_double) {
        player_.doubled = true;
        player_.add(shoe_.draw());
        break; // stand after one card
      } else if (d == Decision::Hit) {
        player_.add(shoe_.draw());
        if (player_.is_bust()) return settle(Outcome::PlayerBust);
        can_double = false; // typically only allowed on first action
      } else {
        // Stand (or illegal Double -> treat as Stand)
        break;
      }
    }

    // DEALER TURN
    // Dealer reveals hole and plays to 17; H17 or S17 per rules
    while (true) {
      int total = dealer_.hard_total();
      bool soft = dealer_.is_soft();
      if (total < 17) {
        dealer_.add(shoe_.draw());
        continue;
      }
      if (total == 17 && rules_.dealer_hits_soft17 && soft) {
        dealer_.add(shoe_.draw());
        continue;
      }
      break;
    }

    // Settle normal outcomes
    if (dealer_.is_bust()) return settle(Outcome::DealerBust);
    int pt = player_.hard_total(), dt = dealer_.hard_total();
    if (pt > dt) return settle(Outcome::PlayerWin);
    if (pt < dt) return settle(Outcome::DealerWin);
    return settle(Outcome::Push);
  }

  const Hand& player() const { return player_; }
  const Hand& dealer() const { return dealer_; }

 private:
  RoundResult settle(Outcome oc) const {
    int64_t payout = 0;
    switch (oc) {
      case Outcome::PlayerBJ:
        payout = bet_ + bet_ * rules_.blackjack_pays_num / rules_.blackjack_pays_den; // returns profit + stake
        break;
      case Outcome::DealerBJ:
      case Outcome::PlayerBust:
        payout = 0;
        break;
      case Outcome::DealerBust:
      case Outcome::PlayerWin:
        payout = bet_ * 2; // profit + stake
        if (player_.doubled) payout = bet_ * 4; // doubled stake returns 2x
        break;
      case Outcome::DealerWin:
        payout = 0; // lose stake
        if (player_.doubled) payout = 0;
        break;
      case Outcome::Push:
        payout = player_.doubled ? bet_ * 2 : bet_; // get stake back
        break;
      case Outcome::PlayerSurrender:
        payout = bet_ / 2; // late surrender: lose half
        break;
    }
    return RoundResult{
      oc,
      player_.hard_total(),
      dealer_.hard_total(),
      payout
    };
  }

  const Rules& rules_;
  Shoe& shoe_;
  Strategy& strat_;
  int64_t bet_;
  mutable Hand player_{}, dealer_{};
};

// ---------- Simple simulation helper ----------
struct SimStats {
  int rounds = 0;
  int player_wins = 0, dealer_wins = 0, pushes = 0, player_bj = 0, dealer_bj = 0, busts = 0, surrenders = 0;
  int64_t bankroll_cents = 0;
};

inline SimStats simulate(int n, const Rules& rules, uint64_t seed=42, int64_t bet_cents=100, Strategy* strategy=nullptr) {
  Shoe shoe{rules.num_decks, seed};
  AlwaysHitUnder17 default_strat;
  Strategy& strat = strategy ? *strategy : default_strat;

  SimStats stats{};
  for (int i=0;i<n;++i) {
    Round r{rules, shoe, strat, bet_cents};
    auto res = r.play();
    stats.rounds++;
    stats.bankroll_cents += (res.payout_cents - (r.player().doubled ? bet_cents*2 : bet_cents)); // net profit/loss
    switch (res.outcome) {
      case Outcome::PlayerBJ: stats.player_bj++; stats.player_wins++; break;
      case Outcome::DealerBJ: stats.dealer_bj++; stats.dealer_wins++; break;
      case Outcome::DealerBust: stats.player_wins++; stats.busts++; break;
      case Outcome::PlayerBust: stats.dealer_wins++; stats.busts++; break;
      case Outcome::PlayerWin: stats.player_wins++; break;
      case Outcome::DealerWin: stats.dealer_wins++; break;
      case Outcome::Push: stats.pushes++; break;
      case Outcome::PlayerSurrender: stats.surrenders++; break;
    }
  }
  return stats;
}

} // namespace bj

