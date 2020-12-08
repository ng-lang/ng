
# ng quickstart

## Hello world!

```
print("Hello world!");
```

## Types

- Integer
- String
- Boolean
- Array
- Object

```
val anInt = 1;

val aString = "string";

val aBoolean = true;

val anArray = [1, 2, 3];

type SimpleObject {
    property prop;
}

val anObject = new SimpleObject { prop: "this is a property" };

```

## Functions

```
fun id(x) {
    return x;
}

fun fact(n) {
    if (n > 0) {
        return n * fact(n-1);
    }
    return n;
}
```

## Member functions

```
val size = "hello".size();

type Person {
    property firstName;
    property lastName;
    fun name() {
        return firstName + " " + lastName;
    }
}

val person = new Person {
    firstName: "Kimmy",
    lastName: "Leo"
};

val name = person.name();
````

## Arrays

```
val simpleArray = [1, 2, 3];

val arrayItem = simpleArray[0];

val twoDimArray = [[1, 2], [3, 4]];

val anotherItem = twoDimArray[1][0];

simpleArray[1] = 5;

simpleArray << 10;
```
