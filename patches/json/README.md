# Description

Read/write helpers for patches using [`http`](../http).

This is not a JSON parser, only meant to retrieve and write fields for http requests.

One call reads one object's own keys. There is no path syntax, but nesting is  
reachable a level at a time. Arrays of objects are walked, arrays of anything  
else come back as raw text.

This is a *library* patch, only other patches need to include this.

# Dependencies

None.

# Authors

- **Creator:** [Kreedy](https://git.gay/kreedy)
  - Github: [Kreedyy](https://github.com/kreedyy) **(either works)**

## Using

The `lrclib` patch is an example/reference implementation.

In your patch's `config.mk` add:

```make
include patches/json/config.mk
```

### Reading one field

The examples below read this, the results lrclib answers when searching  
for `aeter - filthy yet so pretty`. There are two results with the album `clown`:

```json
[
  {
    "id": 22495240, "trackName": "filthy yet so pretty", "artistName": "aeter",
    "albumName": "clown", "duration": 200.0, "instrumental": false,
    "syncedLyrics": "[00:07.27] Filthier, filthier\n[00:10.85] Dead, and I shouldn’t check"
  },

  {
    "id": 22091650, "trackName": "filthy yet so pretty (Paused)", "artistName": "aeter",
    "albumName": "clown", "duration": 200.132, "instrumental": false,
    "syncedLyrics": "[00:07.37] Filthier, filthier, filthier\n[00:11.31] Dead and I shouldn't check"
  }
]
```

```c
/* points one past the final ] */
const char *end = resp->data + resp->len;
char track[256];

if (json_string(resp->data, end, "trackName", track, sizeof(track)))
  /* if true track will now hold trackName, else track[0] = '\0' */
```

### Values that are not strings

`json_string()` only takes strings and returns false for anything else, leaving  
`out` empty. Everything else comes back as a pointer to where the value starts,  
to read on from there. A missing key is NULL here, rather than the empty buffer  
`json_string()` leaves, since there is no position in the body to point at:

```c
/* points one past the final ] */
const char *end = resp->data + resp->len;

const char *p;

p = json_value(resp->data, end, "duration");     /* p = "200.0"  */
duration = (p != NULL) ? strtod(p, NULL) : 0;

p = json_value(resp->data, end, "instrumental"); /* p is at the f of  false  */
instrumental = (p != NULL && *p == 't');         /* a bare true starts with t */

p = json_value(resp->data, end, "nope");         /* the null pointer, no key */
```

Reading the first letter works because lrclib sends a bare `false`.  
If it were quoted, `*p` would be the `"` and this would read false for `"true"`  
and `"false"`, so an api that quotes its booleans wants `json_string()` instead.

This is up to you to handle as you are expected to know the json response structure.

### Walking an array

Both calls above only ever saw the first result. `json_next_object()` hands
over one object at a time, and each one is a range of its own:

```c
/* points one past the final ] */
const char *end = resp->data + resp->len;

const char *obj, *after, *p = resp->data;
char title[256], lyrics[8192];

while ((obj = json_next_object(p, end, &after)) != NULL) {
  /* obj..after spans exactly one { ... }, so these read that result alone */
  json_string(obj, after, "trackName", title, sizeof(title));

  if (strcmp(title, "filthy yet so pretty (Paused)") == 0 &&
      json_string(obj, after, "syncedLyrics", lyrics, sizeof(lyrics)))
    break;

  p = after;    /* where the next object starts */
}
```

The first result is `filthy yet so pretty`, which does not match, so `p = after`  
moves the range past it and the loop goes round. The second pass matches and  
breaks with `lyrics` holding that version's lyrics.  

`json_string()` returning false is normal. An instrumental track  
comes back as `"syncedLyrics": null`, which is not a string, so the call fails  
and empties `lyrics` rather than leaving the pass before it in there. That is  
the reason to keep the return in the condition, otherwise the loop cannot tell  
a track with no synced lyrics from one it has not reached yet.

### Reaching into a nested object

A key only matches on the object it belongs to. `remote` patch reads a nested one:

```json
[
  {
    "sha": "64a947c3a7a3646656b8541e0b463c3a1c4f3edf",
    "commit":
    {
      "message": "Implement a way to add local bindings\n",
      "author":
      {
        "name": "Kreedy",
        "email": "kreedy@..."
      }
    }
  }
]
```

`name` is two levels down, so asking the outer object for it finds nothing:

```c
json_string(resp->data, end, "name", name, sizeof(name));   /* false */
```

Take a step per level instead. `json_value()` returns where the inner object  
starts, and that is what the next call reads from:

```c
/* points one past the final ] */
const char *end = resp->data + resp->len;

const char *commit = json_value(resp->data, end, "commit");
const char *author = (commit != NULL) ? json_value(commit, end, "author")
                                      : NULL;

if (author != NULL)
  json_string(author, end, "name", name, sizeof(name));     /* Kreedy */
```

The outer `end` goes back in every time. The search stops at the inner object's  
own closing brace so it cannot run off into the rest of the body.

### Writing

`json_escape()` is for building a body to POST. It escapes one string and  
leaves the surrounding quotes to the format string. Sending the track above  
back is every string field through it, and the numbers straight in:

```c
char etrack[512], eartist[512], elyrics[16384], body[20000];

json_escape(track, etrack, sizeof(etrack));
json_escape(artist, eartist, sizeof(eartist));

/* lyrics are the field big enough to actually overflow, so check that one */
if (json_escape(lyrics, elyrics, sizeof(elyrics)) >= sizeof(elyrics))
  return;   /* truncated, half a value is not worth sending */

snprintf(body, sizeof(body),
         "{\"trackName\":\"%s\",\"artistName\":\"%s\","
         "\"duration\":%.0f,\"syncedLyrics\":\"%s\"}",
         etrack, eartist, duration, elyrics);

const char *headers[] = { "Content-Type: application/json", NULL };
http_post(url, body, headers, on_done, req);
```

`body` now reads the way the response did:

```json
{
  "trackName":"filthy yet so pretty",
  "artistName":"aeter",
  "duration":200,
  "syncedLyrics":"[00:07.27] Filthier, filthier\n[00:10.85] Dead, and I shouldn’t check"
}
```

The return is the length wanted, so `n` or more means the buffer was too small.
