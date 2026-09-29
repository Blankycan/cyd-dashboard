#pragma once

// Quotes for the quote-of-the-day scene (quote.cpp). Edit freely — keep each
// under ~110 characters so it fits the 240×90 slot in three or four lines.
// UTF-8 is fine for Latin-1 characters (å, ä, ö, é, ...), which the UI font covers.

struct Quote { const char *text; const char *author; };

static const Quote QUOTES[] = {
    { "Waste no more time arguing what a good man should be. Be one.", "Marcus Aurelius" },
    { "The impediment to action advances action. What stands in the way becomes the way.", "Marcus Aurelius" },
    { "We suffer more often in imagination than in reality.", "Seneca" },
    { "Luck is what happens when preparation meets opportunity.", "Seneca" },
    { "Nature does not hurry, yet everything is accomplished.", "Lao Tzu" },
    { "A journey of a thousand miles begins with a single step.", "Lao Tzu" },
    { "No man ever steps in the same river twice.", "Heraclitus" },
    { "Well begun is half done.", "Aristotle" },
    { "Knowing yourself is the beginning of all wisdom.", "Aristotle" },
    { "He who has a why to live can bear almost any how.", "Friedrich Nietzsche" },
    { "Well done is better than well said.", "Benjamin Franklin" },
    { "Lost time is never found again.", "Benjamin Franklin" },
    { "The secret of getting ahead is getting started.", "Mark Twain" },
    { "Any sufficiently advanced technology is indistinguishable from magic.", "Arthur C. Clarke" },
    { "The best way to predict the future is to invent it.", "Alan Kay" },
    { "Simple things should be simple, complex things should be possible.", "Alan Kay" },
    { "Premature optimization is the root of all evil.", "Donald Knuth" },
    { "Talk is cheap. Show me the code.", "Linus Torvalds" },
    { "Make it work, make it right, make it fast.", "Kent Beck" },
    { "First, solve the problem. Then, write the code.", "John Johnson" },
    { "Programs must be written for people to read, and only incidentally for machines to execute.", "Harold Abelson" },
    { "There are only two hard things in computer science: cache invalidation and naming things.", "Phil Karlton" },
    { "Lagom är bäst.", "Swedish proverb" },
    { "Det finns inget dåligt väder, bara dåliga kläder.", "Swedish proverb" },
    { "Borta bra men hemma bäst.", "Swedish proverb" },
    { "Man ska inte sälja skinnet förrän björnen är skjuten.", "Swedish proverb" },
};

static const int QUOTES_N = sizeof(QUOTES) / sizeof(QUOTES[0]);
