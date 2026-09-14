import re


LEXICON = {
    "abandoned": -0.7, "abuse": -0.8, "afraid": -0.7, "aggressive": -0.6,
    "alive": 0.6, "amazing": 0.85, "angry": -0.75, "annoyed": -0.5,
    "anxious": -0.6, "awesome": 0.8, "awful": -0.7, "bad": -0.55,
    "beautiful": 0.75, "best": 0.8, "better": 0.6, "boring": -0.4,
    "broken": -0.6, "calm": 0.4, "can't": -0.2, "cannot": -0.2,
    "celebrate": 0.7, "challenge": -0.2, "cheerful": 0.75, "confused": -0.4,
    "cool": 0.6, "crash": -0.6, "crazy": -0.3, "cry": -0.6, "crying": -0.65,
    "dedicated": 0.5, "defeat": -0.6, "delighted": 0.8, "depressed": -0.8,
    "despair": -0.8, "difficult": -0.4, "disappointed": -0.6, "disgusting": -0.7,
    "doubt": -0.4, "embarrassed": -0.5, "excellent": 0.8, "excited": 0.7,
    "failed": -0.5, "failure": -0.55, "fantastic": 0.8, "fear": -0.7,
    "fine": 0.3, "fired": -0.6, "frustrated": -0.6, "fun": 0.6,
    "glad": 0.6, "good": 0.6, "grateful": 0.75, "great": 0.7,
    "grief": -0.8, "gross": -0.5, "guy": 0.1, "happy": 0.75,
    "hate": -0.8, "hated": -0.75, "healthy": 0.5, "help": 0.5,
    "helpless": -0.7, "honest": 0.6, "hopeless": -0.7, "horrible": -0.75,
    "hurt": -0.6, "impressed": 0.65, "incredible": 0.85, "interested": 0.5,
    "jealous": -0.6, "joy": 0.8, "joyful": 0.8, "killed": -0.7,
    "laugh": 0.6, "laughing": 0.65, "lonely": -0.7, "love": 0.8,
    "loved": 0.8, "lucky": 0.6, "mad": -0.6, "meaningful": 0.5,
    "miss": -0.4, "miserable": -0.8, "nervous": -0.6, "nice": 0.6,
    "no": -0.3, "not": -0.2, "ok": 0.2, "okay": 0.2,
    "pain": -0.8, "panic": -0.75, "perfect": 0.8, "pleased": 0.65,
    "positive": 0.6, "proud": 0.7, "relaxed": 0.5, "relieved": 0.6,
    "sad": -0.7, "safe": 0.5, "scared": -0.7, "shame": -0.6,
    "sick": -0.5, "silly": -0.1, "sorry": -0.3, "stressed": -0.65,
    "strong": 0.5, "struggling": -0.5, "stupid": -0.5, "success": 0.6,
    "sucks": -0.6, "terrible": -0.75, "thankful": 0.7, "thanks": 0.6,
    "tired": -0.3, "tragic": -0.8, "upset": -0.6, "useful": 0.5,
    "victory": 0.7, "wonderful": 0.8, "worried": -0.6, "worst": -0.75,
    "worse": -0.6, "wrong": -0.45,
}

NEGATIONS = {
    "aint", "arent", "cannot", "cant", "couldnt", "darent", "didnt", "doesnt",
    "dont", "hadnt", "hasnt", "havent", "isnt", "mightnt", "mustnt", "neither",
    "never", "no", "nor", "not", "shouldnt", "wasnt", "werent", "wont", "wouldnt",
}

INTENSIFIERS = {
    "absolutely": 1.6, "amazingly": 1.5, "completely": 1.4, "deeply": 1.4,
    "extremely": 1.6, "fully": 1.3, "incredibly": 1.6, "really": 1.3,
    "so": 1.3, "super": 1.5, "terribly": 1.4, "totally": 1.4,
    "truly": 1.4, "unbelievably": 1.5, "utterly": 1.5, "very": 1.3,
}

DIMINISHERS = {
    "a bit": 0.6, "a little": 0.6, "barely": 0.4, "hardly": 0.4,
    "kind of": 0.6, "kinda": 0.6, "slightly": 0.6, "somewhat": 0.7,
}

_ALPHA = 4.0
_TOKEN_RE = re.compile(r"[a-z']+")


class SentimentAnalyzer:
    def score(self, text):
        lowered = text.lower()
        boost = 1.0
        if "!" in text:
            boost += 0.08 * min(text.count("!"), 3)
        tokens = _TOKEN_RE.findall(lowered)

        sum_confirmed = 0.0
        positive_count = 0
        negative_count = 0
        for index, word in enumerate(tokens):
            if word not in LEXICON:
                continue
            valence = LEXICON[word]
            window = tokens[max(0, index - 3):index]
            negation = any(n in window for n in NEGATIONS)
            if negation:
                valence = -valence * 0.75

            factor = 1.0
            for j in range(max(0, index - 2), index):
                if tokens[j] in INTENSIFIERS:
                    factor = max(factor, INTENSIFIERS[tokens[j]])
            for j in range(max(0, index - 2), index):
                if tokens[j] in DIMINISHERS:
                    factor = min(factor, DIMINISHERS[tokens[j]])

            valence *= factor
            sum_confirmed += valence
            if valence > 0:
                positive_count += 1
            elif valence < 0:
                negative_count += 1

        compound = sum_confirmed / ((sum_confirmed * sum_confirmed + _ALPHA) ** 0.5)
        compound *= boost
        compound = max(-1.0, min(1.0, compound))
        return {
            "compound": compound,
            "positive": positive_count,
            "negative": negative_count,
            "count": positive_count + negative_count,
        }