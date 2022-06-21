# Поисковая система
Реализует поиск документов по релевантности. Поддерживает минус-слова, которые исключают из поиска документы, которые их содержат. Ранжирование документов происходит по TF-IDF.
## Демонстрация функционала
База документов:
```   
 Doc id_1:   white cat and yellow hat
 Doc id_2:   curly cat curly tail
 Doc id_3:   nasty dog with big eyes
 Doc id_4:   nasty pigeon john
```

Запрос:
```
and with -john
```

Вывод программы:
```
ACTUAL by default:
{ document_id = 2, relevance = 0.866434, rating = 1 }
{ document_id = 1, relevance = 0.173287, rating = 1 }
{ document_id = 3, relevance = 0.173287, rating = 1 }
BANNED:
{ document_id = 4, relevance = 0.231049, rating = 1 }
```


## Инструкция по развертыванию
* version standard С++17
* Thread Building Blocks (Intel TBB)

## Планы по доработке проекта
Собираюсь провести оптимизацию программы, чтобы ускорить процесс поиска документов.
