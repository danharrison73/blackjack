from .deck import Deck
from .hand import Hand

class BlackjackGame:
    def __init__(self):
        self.deck = Deck()
        self.player_hand = Hand()
        self.dealer_hand = Hand()

    def deal_initial_cards(self):
        for _ in range(2):
            self.player_hand.add_card(self.deck.deal())
            self.dealer_hand.add_card(self.deck.deal())

    def show_hands(self, show_dealer_full=False):
        print("\nYour hand:", self.player_hand)
        print("Your total:", self.player_hand.value)
        if show_dealer_full:
            print("Dealer's hand:", self.dealer_hand)
            print("Dealer's total:", self.dealer_hand.value)
        else:
            print("Dealer's hand: ?,", self.dealer_hand.cards[1])

    def player_turn(self):
        while True:
            choice = input("Do you want to [h]it or [s]tand? ").lower()
            if choice == 'h':
                self.player_hand.add_card(self.deck.deal())
                self.show_hands()
                if self.player_hand.value > 21:
                    print("You bust!")
                    return False
            elif choice == 's':
                return True
            else:
                print("Please enter 'h' or 's'.")

    def dealer_turn(self):
        while self.dealer_hand.value < 17:
            self.dealer_hand.add_card(self.deck.deal())

    def check_winner(self):
        p = self.player_hand.value
        d = self.dealer_hand.value
        if d > 21 or p > d:
            print("You win!")
        elif p < d:
            print("Dealer wins.")
        else:
            print("It's a draw.")

    def play(self):
        print("Welcome to Blackjack!")
        self.deal_initial_cards()
        self.show_hands()

        if not self.player_turn():
            return

        self.dealer_turn()
        self.show_hands(show_dealer_full=True)
        self.check_winner()
