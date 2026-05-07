
## Translating the KeeperFX Launcher

KeeperFX has players all over the world.
There are quite some players that do not understand English and
we would like to be able to help them enjoy the game.
For this we translate the game and the launcher into the most common languages.

The KeeperFX launcher uses the GNU Gettext format (PO and POT files) to be consistent with KeeperFX.
POT files are the translation templates from which you can make a translation file for a specific language (PO file).

The best way of translating is to use a tool such as [POEdit](https://poedit.net/).
There are many other tools but this is the one most of our translators use.
You can find instructions on how to use POEdit below.

If you need help with specific translations, using both [DeepL](https://www.deepl.com) and [ChatGPT](https://chatgpt.com/)
together with your own understanding of the language seems to be an easy way to get the correct translations. 
You could also contact other translators of the language to get some input on your translations.

If any translations are not clear or you don't know how they are used,
feel free to ask us in the [Keeper Klan Discord](https://discord.gg/hE4p7vy2Hb).
We're always there to help you out.

Feel free to submit PRs for translations or
contact Yani on the Keeper Klan Discord to submit any translations you might have.



### Languages and Translators

The current languages that we are translating for the Launcher are
the ones that are also present in KeeperFX.
Other languages are allowed too, but if they are not in KeeperFX we might
hold out on adding them until the game also supports the language.

| Language              | Code     | Completed | Done/Total     | Translator(s)
|-----------------------|----------|-----------|----------------|--------------
| English               | EN       | 100%      | Base           | [Yani](https://github.com/yani)
| Dutch                 | NL       | 100%      | 338/338        | [Yani](https://github.com/yani)
| Italian               | IT       | 100%      | 338/338        | [kammerer](https://github.com/Kammerer001)
| French                | FR       | 100%      | 338/338        | AncientWay, [Thorgal](https://github.com/Thorgal93)
| Spanish               | ES       | 100%      | 338/338        | [Alniarez](https://github.com/alniarez)
| German                | DE       | 89.6%     | 303/338        | Aqua, Dofi
| Polish                | PL       | 95.9%     | 324/338        | [Rusty](https://github.com/rustyspoonz)
| Swedish               | SV       |           |                | 
| Japanese              | JA       | 88%       | 300/341        | [AdamPlenty](https://github.com/AdamPlenty)
| Russian               | RU       | 97.7%     | 333/341        | Quuz, [kammerer](https://github.com/Kammerer001)
| Korean                | KO       | 97.7%     | 333/341        | Desert
| Chinese (Simplified)  | ZH-HANS  | 83.6%     | 285/341        | [JieLeTian](https://github.com/jieletian)
| Chinese (Traditional) | ZH-HANT  |           |                | 
| Czech                 | CS       | 83.9%     | 286/341        | Gotrek
| Latin                 | LA       |           |                | 
| Ukrainian             | UK       | 97.7%     | 333/341        | Mr.Negative, Renegade_Glitch
| Portuguese (Brazil)   | PT       | 83.9%     | 286/341        | [altiereslima](https://github.com/altiereslima)



### Translating with POEdit

First download the [i18n/translations.pot](../i18n/translations.pot) file which is the template for the translations.

Then open up POEdit and start a new translation by selecting `File > New From POT/PO File...`
and selecting the `translations.pot` file.
You can then start translating all of the strings to the language you want to help us with.

Be sure to check out the keybinds under the `Go` menu item. You can mark the current translation as Done
and then move to the next one that needs work by pressing `Ctrl+Return`.
This is a good one to know.

When new translation strings are added or when existing ones are updated,
you can select `Translation > Update from POT file...` to update the list. 
POEdit will mark the ones that need work.



### Some notes

- If you come across translations that use an ampersand (`&`) it means that this translation is also used for a shortcut.
These shortcuts are common for buttons and menu items.
They are added to the translations because they might be different in other languages.
Some languages also use a different alphabet and add the shortcut keys separately.
For example in Japanese: 
    - `&Yes` -> `はい（&Y）`
    - `&No` -> `いいえ（&N）`

- Another thing that's important is that some translations are better if they use the English word.
Here's a list of terms that are best kept in English because we use them a lot during international
discussion or when the developers are troubleshooting a problem the user might have:
    - Launcher
    - Workshop
    - Cheats
    - Screenshot
    - Backup
    - Packetsave
    - Heavylog

- If your language uses gendered translations, it's best to rewrite it in a gender-neutral way so it always makes sense.
Translations don't need to be perfect 1:1 translations so feel free to rewrite them in way that feels natural.
You can look at common UI translations in your language if you are unsure.



### Testing your translation

It's possible to load a language file directly into the launcher with the following command line option:
```
keeperfx-launcher-qt.exe --language-file=my_translation.po
```



### Thanks

Translations are a community effort and we're very thankful for anybody who can help us out.
