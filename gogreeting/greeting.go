package greeting

import (
	"errors"
	"fmt"
)

func Hello(name string) (string, error) {
    // If no name was given, return an error with a message.
    if name == "" {
        return name, errors.New("empty name")
    }
    // Create a message using a random format.
    // message := fmt.Sprintf(randomFormat(), name)
    message := fmt.Sprint(randomFormat())
    return message, nil
}

func randomFormat()string {
	return "Xiangzhe Zhang"
}