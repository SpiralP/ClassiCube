﻿// ClassicalSharp copyright 2014-2016 UnknownShadow200 | Licensed under MIT
using System;
using System.Drawing;
using System.IO;
using ClassicalSharp.Generator;
using ClassicalSharp.Gui.Widgets;
using ClassicalSharp.Singleplayer;
using OpenTK.Input;

namespace ClassicalSharp.Gui.Screens {
	public sealed class GenLevelScreen : MenuScreen {
		
		public GenLevelScreen( Game game ) : base( game ) {
		}

		MenuInputWidget selectedWidget;
		
		public override bool HandlesMouseClick( int mouseX, int mouseY, MouseButton button ) {
			return HandleMouseClick( widgets, mouseX, mouseY, button );
		}
		
		public override bool HandlesKeyPress( char key ) {
			return selectedWidget == null ? true :
				selectedWidget.HandlesKeyPress( key );
		}
		
		public override bool HandlesKeyDown( Key key ) {
			if( key == Key.Escape ) {
				game.Gui.SetNewScreen( null );
				return true;
			}
			return selectedWidget == null ? (key < Key.F1 || key > Key.F35) :
				selectedWidget.HandlesKeyDown( key );
		}
		
		public override bool HandlesKeyUp( Key key ) {
			return selectedWidget == null ? true :
				selectedWidget.HandlesKeyUp( key );
		}
		
		public override void Init() {
			game.Keyboard.KeyRepeat = true;
			titleFont = new Font( game.FontName, 16, FontStyle.Bold );
			regularFont = new Font( game.FontName, 16, FontStyle.Regular );
			
			widgets = new Widget[] {
				MakeInput( -80, false, game.World.Width.ToString() ),
				MakeInput( -40, false, game.World.Height.ToString() ),
				MakeInput( 0, false, game.World.Length.ToString() ),
				MakeInput( 40, true, "" ),
				
				MakeLabel( -150, -80, "Width:" ), MakeLabel( -150, -40, "Height:" ),
				MakeLabel( -150, 0, "Length:" ), MakeLabel( -140, 40, "Seed:" ),
				TextWidget.Create( game, "Generate new level", regularFont )
					.SetLocation( Anchor.Centre, Anchor.Centre, 0, -130 ),
				
				ButtonWidget.Create( game, 201, 40, "Flatgrass", titleFont, GenFlatgrassClick )
					.SetLocation( Anchor.Centre, Anchor.Centre, -120, 100 ),
				ButtonWidget.Create( game, 201, 40, "Vanilla",  titleFont, GenNotchyClick )
					.SetLocation( Anchor.Centre, Anchor.Centre, 120, 100 ),
				MakeBack( false, titleFont,
				         (g, w) => g.Gui.SetNewScreen( new PauseScreen( g ) ) ),
			};
		}
		
		MenuInputWidget MakeInput( int y, bool seed, string value ) {
			MenuInputValidator validator = seed ? new SeedValidator() : new IntegerValidator( 1, 8192 );
			MenuInputWidget widget = MenuInputWidget.Create(
				game, 0, y, 200, 30, value, Anchor.Centre, Anchor.Centre,
				regularFont, titleFont, validator );
			widget.Active = false;
			widget.OnClick = InputClick;
			return widget;
		}
		
		TextWidget MakeLabel( int x, int y, string text ) {
			TextWidget widget = TextWidget.Create( game, text, regularFont )
				.SetLocation( Anchor.Centre, Anchor.Centre, x, y );
			
			widget.XOffset = -110 - widget.Width / 2;
			widget.CalculatePosition();
			widget.Colour = new FastColour( 224, 224, 224 );
			return widget;
		}
		
		public override void Dispose() {
			game.Keyboard.KeyRepeat = false;
			base.Dispose();
		}
		
		void InputClick( Game game, Widget widget, MouseButton mouseBtn ) {
			if( mouseBtn != MouseButton.Left ) return;
			if( selectedWidget != null )
				selectedWidget.Active = false;
			
			selectedWidget = (MenuInputWidget)widget;
			selectedWidget.Active = true;
		}
		
		void GenFlatgrassClick( Game game, Widget widget, MouseButton mouseBtn ) {
			if( mouseBtn != MouseButton.Left ) return;
			GenerateMap( new FlatGrassGenerator() );
		}
		
		void GenNotchyClick( Game game, Widget widget, MouseButton mouseBtn ) {
			if( mouseBtn != MouseButton.Left ) return;
			GenerateMap( new NotchyGenerator() );
		}
		
		void GenerateMap( IMapGenerator gen ) {
			SinglePlayerServer server = (SinglePlayerServer)game.Server;
			int width = GetInt( 0 ), height = GetInt( 1 );
			int length = GetInt( 2 ), seed = GetSeedInt( 3 );
			
			long volume = (long)width * height * length;
			if( volume > 800 * 800 * 800 ) {
				game.Chat.Add( "&cThe generated map's volume is too big." );
			} else if( width == 0 || height == 0 || length == 0 ) {
				game.Chat.Add( "&cOne of the map dimensions is invalid.");
			} else {
				server.GenMap( width, height, length, seed, gen );
			}
		}
		
		int GetInt( int index ) {
			MenuInputWidget input = (MenuInputWidget)widgets[index];
			string text = input.GetText();
			if( !input.Validator.IsValidValue( text ) )
				return 0;
			return text == "" ? 0 : Int32.Parse( text );
		}
		
		int GetSeedInt( int index ) {
			MenuInputWidget input = (MenuInputWidget)widgets[index];
			string text = input.GetText();
			if( text == "" ) return new Random().Next();
			
			if( !input.Validator.IsValidValue( text ) )
				return 0;
			return text == "" ? 0 : Int32.Parse( text );
		}
	}
}