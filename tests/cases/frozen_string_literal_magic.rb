# frozen_string_literal: true

str = "hello"
p str.frozen?
p "world".frozen?

begin
  "hello" << " world"
rescue => e
  p e.class
end
