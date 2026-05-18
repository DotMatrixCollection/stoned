require "bundler"

module Bundler
  module FriendlyErrors
    class << self
      def with_friendly_errors
        yield if block_given?
      end
    end
  end
end
